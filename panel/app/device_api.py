"""ESP32 <-> panel protokolu. Sadece outbound HTTPS; ESP32'de port acilmaz."""
from datetime import datetime, timezone

from fastapi import APIRouter, Depends, Header, HTTPException
from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.orm import Session

from .db import Card, Device, Event, Session_, Setting, get_db

router = APIRouter(prefix="/api/v1", tags=["device"])


def _device(db: Session, token: str) -> Device:
    dev = db.scalar(select(Device).where(Device.api_token == token))
    if not dev:
        raise HTTPException(status_code=401, detail="gecersiz token")
    return dev


def device_auth(authorization: str | None = Header(default=None),
                db: Session = Depends(get_db)) -> Device:
    """Kimlik ONCE dogrulanir; govde semasi sonra. Yetkisiz istek 422 degil 401 alir."""
    tok = (authorization or "").replace("Bearer ", "").strip()
    if not tok:
        raise HTTPException(status_code=401, detail="token yok",
                            headers={"WWW-Authenticate": "Bearer"})
    return _device(db, tok)


# ------------------------------------------------------------------ semalar
class HeartbeatIn(BaseModel):
    firmware: str = ""
    ip: str = ""
    state: str = ""
    uptime_s: int = 0
    free_heap: int = 0
    card_count: int = 0
    # canli sarj verisi (panelde gosterilir)
    power_kw: float = 0.0
    session_kwh: float = 0.0
    current_a: int = 0
    temp_c: float = 0.0
    session_min: int = 0
    cmd_id: int = 0            # uygulanan son komut id'si


class SessionIn(BaseModel):
    uid: str = Field(min_length=2, max_length=24)
    # ESP32 kuyrugu tekrar denedigi icin ayni kayit iki kez gelebilir
    client_id: str = Field(default="", max_length=48)
    started_at: datetime | None = None
    ended_at: datetime | None = None
    kwh: float = 0.0
    reason: int = 0


class UnknownIn(BaseModel):
    uid: str = Field(min_length=2, max_length=24)


class EnrollIn(BaseModel):
    uid: str = Field(min_length=4, max_length=24)
    name: str = Field(default="KART", max_length=32)


# ------------------------------------------------------------------ uclar
@router.post("/heartbeat")
def heartbeat(body: HeartbeatIn,
              dev: Device = Depends(device_auth),
              db: Session = Depends(get_db)):
    dev.last_seen = datetime.utcnow()
    dev.firmware = body.firmware[:24]
    dev.ip = body.ip[:40]
    dev.state = body.state[:24]
    dev.power_kw = body.power_kw
    dev.session_kwh = body.session_kwh
    dev.current_a = body.current_a
    dev.temp_c = body.temp_c
    dev.session_min = body.session_min
    if body.cmd_id:
        dev.cmd_ack = body.cmd_id
    db.commit()

    # Komut/rev'i heartbeat yanitinda da ver: ESP32'nin 60 sn'lik config
    # turunu beklemeden "durdur" komutu hemen uygulanir.
    st = dev.settings
    return {
        "ok": True,
        "server_time": datetime.utcnow().isoformat(),
        "rev": st.rev if st else 0,
        "cmd": st.cmd if st else "",
        "cmd_id": st.cmd_id if st else 0,
    }


@router.get("/config")
def config(dev: Device = Depends(device_auth),
           db: Session = Depends(get_db)):
    """ESP32 son bilinen listeyi NVS'ye yazar. VPS dususe fail-closed calisir."""
    st = dev.settings
    if not st:
        st = Setting(device_id=dev.id)
        db.add(st)
        db.commit()

    cards = db.scalars(select(Card).where(Card.device_id == dev.id)).all()
    return {
        "rev": st.rev,
        "device": {
            "name": dev.name,
            "tuya_id": dev.tuya_id or "",
            # LAN kontrolu icin AES anahtari. Sadece Bearer token'li
            # cihaz isteklerinde doner (HTTPS uzerinden).
            "local_key": dev.local_key or "",
        },
        "settings": {
            "max_a": st.max_a,
            "def_a": st.def_a,
            "require_card": st.require_card,
            "schedule_en": st.schedule_en,
            "schedule_start": st.schedule_start,
            "schedule_end": st.schedule_end,
            "schedule_days": st.schedule_days,
            "max_session_min": st.max_session_min,
            "target_kwh": st.target_kwh,
            "cmd": st.cmd,
            "cmd_id": st.cmd_id,
        },
        "cards": [
            {"uid": c.uid, "name": c.name, "admin": c.is_admin,
             "active": c.active, "quota_kwh": c.quota_kwh,
             "used_kwh": round(c.used_kwh, 1)}
            for c in cards
        ],
    }


@router.post("/session")
def push_session(body: SessionIn,
                 dev: Device = Depends(device_auth),
                 db: Session = Depends(get_db)):

    card = db.scalar(select(Card).where(Card.device_id == dev.id,
                                        Card.uid == body.uid))

    def _naive_utc(v):
        if v is None:
            return datetime.utcnow()
        if v.tzinfo is not None:
            v = v.astimezone(timezone.utc).replace(tzinfo=None)
        return v

    # mukerrer gonderim: ayni client_id ile geldiyse yeni kayit acma
    if body.client_id:
        dup = db.scalar(select(Session_).where(Session_.client_id == body.client_id))
        if dup:
            return {"ok": True, "id": dup.id, "dup": True}

    end = _naive_utc(body.ended_at)
    start = _naive_utc(body.started_at) if body.started_at else None

    rec = Session_(
        device_id=dev.id,
        card_id=card.id if card else None,
        uid=body.uid,
        name=card.name if card else "",
        started_at=start,
        ended_at=end,
        created_at=end,
        kwh=round(body.kwh, 3),
        reason=body.reason,
        client_id=body.client_id,
    )
    db.add(rec)

    if card:
        card.used_kwh = round(card.used_kwh + body.kwh, 2)
        card.last_used_at = end

    db.commit()
    return {"ok": True, "id": rec.id}


@router.post("/card")
def enroll_card(body: EnrollIn,
                dev: Device = Depends(device_auth),
                db: Session = Depends(get_db)):
    """ESP32'de okutulan yeni karti panele kaydeder (admin menusunden)."""

    uid = body.uid.replace(":", "").replace(" ", "").upper()
    if len(uid) < 4 or any(c not in "0123456789ABCDEF" for c in uid):
        raise HTTPException(status_code=400, detail="gecersiz uid")

    if db.scalar(select(Card).where(Card.device_id == dev.id, Card.uid == uid)):
        return {"ok": True, "duplicate": True}

    db.add(Card(device_id=dev.id, uid=uid, name=body.name[:32], quota_kwh=0))
    if dev.settings:
        dev.settings.rev += 1
    db.add(Event(device_id=dev.id, kind="card_enroll", detail=uid))
    db.commit()
    return {"ok": True, "rev": dev.settings.rev if dev.settings else 0}


@router.post("/unknown")
def unknown_card(body: UnknownIn,
                 dev: Device = Depends(device_auth),
                 db: Session = Depends(get_db)):
    db.add(Event(device_id=dev.id, kind="unknown_card", detail=body.uid))
    db.commit()
    return {"ok": True}
