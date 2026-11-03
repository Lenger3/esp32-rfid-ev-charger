import os
import secrets
from datetime import datetime, timedelta, timezone

from fastapi import Depends, FastAPI, Form, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from sqlalchemy import func, select
from sqlalchemy.orm import Session

from .db import Card, Device, Event, Session_, Setting, SessionLocal, get_db, init_db
from .device_api import router as device_router
from . import ratelimit
from .security import (COOKIE_NAME, SESSION_HOURS, check_password, make_token,
                       verify_token)

HERE = os.path.dirname(__file__)

app = FastAPI(title="Sarj Paneli", docs_url=None, redoc_url=None)
app.include_router(device_router)
app.mount("/static", StaticFiles(directory=os.path.join(HERE, "static")),
          name="static")
templates = Jinja2Templates(directory=os.path.join(HERE, "templates"))

REASON_TR = {0: "normal", 4: "iptal", 5: "taninmayan", 6: "elektrik kesintisi"}


from zoneinfo import ZoneInfo

LOCAL_TZ = ZoneInfo(os.getenv("TZ_NAME", "Europe/Istanbul"))


def fmt_dt(v):
    """DB'de UTC tutulur, kullaniciya yerel saat gosterilir."""
    if not v:
        return "—"
    if v.tzinfo is None:
        v = v.replace(tzinfo=timezone.utc)
    return v.astimezone(LOCAL_TZ).strftime("%d.%m.%Y %H:%M")


templates.env.filters["dt"] = fmt_dt


@app.on_event("startup")
def _startup() -> None:
    init_db()
    try:
        from .migrate import main as migrate
        migrate()
    except Exception as e:                       # goc basarisizsa panel yine acilsin
        print(f"[migrate] UYARI: {e}", flush=True)
    with SessionLocal() as db:
        _ensure_device(db)


def _ensure_device(db: Session) -> Device:
    dev = db.scalar(select(Device).limit(1))
    if dev:
        return dev
    token = os.getenv("DEVICE_TOKEN") or secrets.token_urlsafe(32)
    dev = Device(name="EVCHARGE-01",
                 tuya_id=os.getenv("TUYA_DEVICE_ID", ""),
                 api_token=token)
    dev.settings = Setting()
    db.add(dev)
    db.commit()
    db.refresh(dev)
    print(f"\n*** CIHAZ TOKENI (firmware config.h -> API_TOKEN): {token}\n", flush=True)
    return dev


def guard(request: Request) -> None:
    if not verify_token(request.cookies.get(COOKIE_NAME)):
        raise HTTPException(status_code=303, headers={"Location": "/login"})


# ------------------------------------------------------------------ auth
def _client_ip(request: Request) -> str:
    """Caddy arkadasinda gercek IP X-Real-IP'de."""
    xr = request.headers.get("x-real-ip")
    if xr:
        return xr.strip()
    xf = request.headers.get("x-forwarded-for")
    if xf:
        return xf.split(",")[0].strip()
    return request.client.host if request.client else "?"


@app.get("/login", response_class=HTMLResponse)
def login_page(request: Request):
    locked, wait = ratelimit.is_locked(_client_ip(request))
    if locked:
        return templates.TemplateResponse(
            "login.html", {"request": request,
                           "hata": f"Cok fazla deneme. {wait // 60 + 1} dk sonra tekrar dene."},
            status_code=429)
    return templates.TemplateResponse("login.html", {"request": request, "hata": None})


@app.post("/login")
def login(request: Request, sifre: str = Form(...)):
    ip = _client_ip(request)
    locked, wait = ratelimit.is_locked(ip)
    if locked:
        return templates.TemplateResponse(
            "login.html", {"request": request,
                           "hata": f"Cok fazla deneme. {wait // 60 + 1} dk sonra tekrar dene."},
            status_code=429)

    if not check_password(sifre):
        ratelimit.record_fail(ip)
        return templates.TemplateResponse(
            "login.html", {"request": request, "hata": "Hatali sifre"},
            status_code=401)

    ratelimit.clear(ip)

    resp = RedirectResponse("/", status_code=303)
    resp.set_cookie(COOKIE_NAME, make_token(), httponly=True, samesite="lax",
                    max_age=SESSION_HOURS * 3600,
                    secure=os.getenv("COOKIE_SECURE", "1") == "1")
    return resp


@app.get("/logout")
def logout():
    resp = RedirectResponse("/login", status_code=303)
    resp.delete_cookie(COOKIE_NAME)
    return resp


# ------------------------------------------------------------------ panel
@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request, db: Session = Depends(get_db),
              _=Depends(guard)):
    dev = _ensure_device(db)
    now = datetime.utcnow()
    month_start = now.replace(day=1, hour=0, minute=0, second=0, microsecond=0)

    month_kwh = db.scalar(select(func.coalesce(func.sum(Session_.kwh), 0.0))
                          .where(Session_.created_at >= month_start)) or 0.0
    today_kwh = db.scalar(select(func.coalesce(func.sum(Session_.kwh), 0.0))
                          .where(Session_.created_at >= now - timedelta(days=1))) or 0.0
    n_cards = db.scalar(select(func.count(Card.id))) or 0

    recent = db.scalars(select(Session_).order_by(Session_.created_at.desc())
                        .limit(12)).all()
    cards = db.scalars(select(Card).order_by(Card.used_kwh.desc()).limit(6)).all()

    return templates.TemplateResponse("dashboard.html", {
        "request": request, "dev": dev, "month_kwh": round(month_kwh, 1),
        "today_kwh": round(today_kwh, 1), "n_cards": n_cards,
        "recent": recent, "cards": cards, "reasons": REASON_TR, "page": "home",
        "s": dev.settings,
        "msg": request.query_params.get("msg"),
    })


@app.get("/users", response_class=HTMLResponse)
def users(request: Request, db: Session = Depends(get_db), _=Depends(guard)):
    _ensure_device(db)
    cards = db.scalars(select(Card).order_by(Card.name)).all()
    return templates.TemplateResponse("users.html", {
        "request": request, "cards": cards, "page": "users",
        "msg": request.query_params.get("msg")})


@app.post("/users/add")
def user_add(uid: str = Form(...), name: str = Form(...),
             quota_kwh: int = Form(0), is_admin: bool = Form(False),
             db: Session = Depends(get_db), _=Depends(guard)):
    dev = _ensure_device(db)
    uid_norm = uid.replace(":", "").replace(" ", "").upper()

    if len(uid_norm) < 4 or any(c not in "0123456789ABCDEF" for c in uid_norm):
        return RedirectResponse("/users?msg=UID+gecersiz+(hex+olmali)", status_code=303)

    if db.scalar(select(Card).where(Card.uid == uid_norm)):
        return RedirectResponse("/users?msg=Bu+kart+zaten+kayitli", status_code=303)

    db.add(Card(device_id=dev.id, uid=uid_norm, name=name[:32],
                quota_kwh=max(0, quota_kwh), is_admin=is_admin))
    if dev.settings:
        dev.settings.rev += 1
    db.add(Event(device_id=dev.id, kind="card_add", detail=f"{uid_norm} {name}"))
    db.commit()
    return RedirectResponse("/users?msg=Kart+eklendi", status_code=303)


@app.post("/users/{card_id}/update")
def user_update(card_id: int, name: str = Form(...), quota_kwh: int = Form(0),
                is_admin: bool = Form(False), active: bool = Form(False),
                db: Session = Depends(get_db), _=Depends(guard)):
    card = db.get(Card, card_id)
    if not card:
        raise HTTPException(404)
    card.name = name[:32]
    card.quota_kwh = max(0, quota_kwh)
    card.is_admin = is_admin
    card.active = active
    if card.device.settings:
        card.device.settings.rev += 1
    db.add(Event(device_id=card.device_id, kind="card_update", detail=card.uid))
    db.commit()
    return RedirectResponse("/users?msg=Guncellendi", status_code=303)


@app.post("/users/{card_id}/reset")
def user_reset(card_id: int, db: Session = Depends(get_db), _=Depends(guard)):
    card = db.get(Card, card_id)
    if not card:
        raise HTTPException(404)
    card.used_kwh = 0.0
    if card.device.settings:
        card.device.settings.rev += 1
    db.add(Event(device_id=card.device_id, kind="card_reset", detail=card.uid))
    db.commit()
    return RedirectResponse("/users?msg=Kullanim+sifirlandi", status_code=303)


@app.post("/users/{card_id}/delete")
def user_delete(card_id: int, db: Session = Depends(get_db), _=Depends(guard)):
    card = db.get(Card, card_id)
    if not card:
        raise HTTPException(404)
    uid, dev_id = card.uid, card.device_id
    db.delete(card)
    dev = db.get(Device, dev_id)
    if dev and dev.settings:
        dev.settings.rev += 1
    db.add(Event(device_id=dev_id, kind="card_delete", detail=uid))
    db.commit()
    return RedirectResponse("/users?msg=Kart+silindi", status_code=303)


@app.get("/sessions", response_class=HTMLResponse)
def sessions(request: Request, limit: int = 100, db: Session = Depends(get_db),
             _=Depends(guard)):
    rows = db.scalars(select(Session_).order_by(Session_.created_at.desc())
                      .limit(min(limit, 500))).all()
    total = sum(r.kwh for r in rows)
    return templates.TemplateResponse("sessions.html", {
        "request": request, "rows": rows, "reasons": REASON_TR,
        "total": round(total, 2), "page": "sessions"})


@app.post("/charge/start")
def charge_start(target_kwh: float = Form(0.0), amps: int = Form(0),
                 db: Session = Depends(get_db), _=Depends(guard)):
    """Uzaktan sarj baslat. Kart okutulmadan baslar (yonetici komutu)."""
    dev = _ensure_device(db)
    s = dev.settings
    if not s:
        s = Setting(device_id=dev.id)
        db.add(s)

    if amps:
        s.def_a = min(s.max_a, max(6, amps))
    s.target_kwh = min(200.0, max(0.0, target_kwh))
    s.cmd = "start"
    s.cmd_id += 1
    s.rev += 1

    detail = f"{s.def_a}A"
    if s.target_kwh > 0:
        detail += f" {s.target_kwh:.1f}kWh"
    db.add(Event(device_id=dev.id, kind="charge_start", detail=detail))
    db.commit()
    return RedirectResponse("/?msg=Sarj+komutu+gonderildi", status_code=303)


@app.post("/charge/stop")
def charge_stop(db: Session = Depends(get_db), _=Depends(guard)):
    dev = _ensure_device(db)
    s = dev.settings
    if not s:
        s = Setting(device_id=dev.id)
        db.add(s)
    s.cmd = "stop"
    s.cmd_id += 1
    s.rev += 1
    db.add(Event(device_id=dev.id, kind="charge_stop", detail=""))
    db.commit()
    return RedirectResponse("/?msg=Durdurma+komutu+gonderildi", status_code=303)


@app.get("/settings", response_class=HTMLResponse)
def settings_page(request: Request, db: Session = Depends(get_db),
                  _=Depends(guard)):
    dev = _ensure_device(db)
    st = dev.settings or Setting(device_id=dev.id)
    # Jinja2 bitwise & desteklemez -> gun maskesini burada cozelim
    daychecked = [bool(st.schedule_days // (2 ** i) % 2) for i in range(7)]
    return templates.TemplateResponse("settings.html", {
        "request": request, "dev": dev, "s": st, "page": "settings",
        "daychecked": daychecked,
        "msg": request.query_params.get("msg")})


@app.post("/settings")
def settings_save(max_a: int = Form(16), def_a: int = Form(10),
                  require_card: bool = Form(False),
                  schedule_en: bool = Form(False),
                  schedule_start: str = Form("22:00"),
                  schedule_end: str = Form("06:00"),
                  max_session_min: int = Form(480),
                  target_kwh: float = Form(0.0),
                  days: list[int] = Form([]),
                  db: Session = Depends(get_db), _=Depends(guard)):
    dev = _ensure_device(db)
    s = dev.settings
    if not s:
        s = Setting(device_id=dev.id)
        db.add(s)

    s.max_a = min(32, max(6, max_a))
    s.def_a = min(s.max_a, max(6, def_a))
    s.require_card = require_card
    s.schedule_en = schedule_en
    s.schedule_start = schedule_start[:5]
    s.schedule_end = schedule_end[:5]
    s.max_session_min = min(1440, max(0, max_session_min))
    s.target_kwh = min(200.0, max(0.0, target_kwh))

    bits = 0
    for d in days:
        if 0 <= d <= 6:
            bits |= (1 << d)
    s.schedule_days = bits or 127
    s.rev += 1

    db.add(Event(device_id=dev.id, kind="settings", detail=f"rev={s.rev}"))
    db.commit()
    return RedirectResponse("/settings?msg=Kaydedildi", status_code=303)


@app.get("/device", response_class=HTMLResponse)
def device_page(request: Request, db: Session = Depends(get_db),
                _=Depends(guard)):
    dev = _ensure_device(db)
    events = db.scalars(select(Event).order_by(Event.created_at.desc())
                        .limit(50)).all()
    return templates.TemplateResponse("device.html", {
        "request": request, "dev": dev, "events": events, "page": "device",
        "msg": request.query_params.get("msg"),
        "err": request.query_params.get("err")})


@app.post("/device")
def device_save(name: str = Form("EVCHARGE-01"), tuya_id: str = Form(""),
                local_key: str = Form(""), clear_local_key: str = Form(""),
                db: Session = Depends(get_db), _=Depends(guard)):
    dev = _ensure_device(db)

    name = name.strip()[:64] or "EVCHARGE-01"
    tid = tuya_id.strip().lower()
    lkey = local_key.strip()

    # Tuya cihaz ID: kucuk harf + rakam, 8-32 karakter (ornek: 0123456789abcdef0123)
    if tid and (not (8 <= len(tid) <= 32) or not tid.isalnum()):
        return RedirectResponse(
            "/device?err=Tuya+ID+8-32+karakter+olmali+ve+sadece+harf/rakam+icermeli",
            status_code=303)

    # local_key: Tuya 16 karakterlik AES anahtari, yazdirilabilir ASCII
    if lkey and len(lkey) != 16:
        return RedirectResponse(
            f"/device?err=local_key+16+karakter+olmali+(girilen:+{len(lkey)})",
            status_code=303)

    changed = []
    if dev.name != name:
        changed.append(f"ad: {dev.name} -> {name}")
        dev.name = name
    if (dev.tuya_id or "") != tid:
        changed.append(f"tuya_id: {dev.tuya_id or '-'} -> {tid or '-'}")
        dev.tuya_id = tid

    if clear_local_key:
        if dev.local_key:
            changed.append("local_key silindi")
            dev.local_key = ""
    elif lkey and lkey != (dev.local_key or ""):
        # siri log'a yazma — sadece "degistirildi"
        changed.append("local_key degistirildi")
        dev.local_key = lkey

    if changed:
        db.add(Event(device_id=dev.id, kind="device_update",
                     detail="; ".join(changed)[:400]))
        db.commit()
        return RedirectResponse("/device?msg=Kaydedildi", status_code=303)

    return RedirectResponse("/device?msg=Degisiklik+yok", status_code=303)


@app.post("/device/rotate-token")
def rotate_token(db: Session = Depends(get_db), _=Depends(guard)):
    dev = _ensure_device(db)
    dev.api_token = secrets.token_urlsafe(32)
    db.add(Event(device_id=dev.id, kind="token_rotate", detail=""))
    db.commit()
    return RedirectResponse("/device?msg=Yeni+token+olusturuldu", status_code=303)


@app.get("/health")
def health(db: Session = Depends(get_db)):
    db.execute(select(1))
    return {"ok": True}
