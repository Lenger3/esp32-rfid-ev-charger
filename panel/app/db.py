import os
from datetime import datetime

from sqlalchemy import (Boolean, DateTime, Float, ForeignKey, Integer, String,
                        Text, create_engine, func)
from sqlalchemy.orm import (DeclarativeBase, Mapped, Session, mapped_column,
                            relationship, sessionmaker)

DATABASE_URL = os.getenv("DATABASE_URL", "sqlite:///./panel.db")

connect_args = {"check_same_thread": False} if DATABASE_URL.startswith("sqlite") else {}
engine = create_engine(DATABASE_URL, pool_pre_ping=True, connect_args=connect_args)
SessionLocal = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False)


class Base(DeclarativeBase):
    pass


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(64), default="EVCHARGE-01")
    tuya_id: Mapped[str] = mapped_column(String(64), default="")
    # Tuya LAN protokolu icin AES anahtari — SIR, panelde maskelenir
    local_key: Mapped[str] = mapped_column(String(32), default="")
    api_token: Mapped[str] = mapped_column(String(96), unique=True, index=True)
    firmware: Mapped[str] = mapped_column(String(24), default="")
    ip: Mapped[str] = mapped_column(String(40), default="")
    state: Mapped[str] = mapped_column(String(24), default="")
    last_seen: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    # canli sarj verisi (son heartbeat)
    power_kw: Mapped[float] = mapped_column(Float, default=0.0)
    session_kwh: Mapped[float] = mapped_column(Float, default=0.0)
    current_a: Mapped[int] = mapped_column(Integer, default=0)
    temp_c: Mapped[float] = mapped_column(Float, default=0.0)
    session_min: Mapped[int] = mapped_column(Integer, default=0)
    cmd_ack: Mapped[int] = mapped_column(Integer, default=0)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    cards: Mapped[list["Card"]] = relationship(back_populates="device",
                                               cascade="all, delete-orphan")
    settings: Mapped["Setting | None"] = relationship(back_populates="device",
                                                      uselist=False,
                                                      cascade="all, delete-orphan")

    @property
    def online(self) -> bool:
        if not self.last_seen:
            return False
        # ESP32 30 sn'de bir heartbeat atar; 3 kacirilirsa cevrimdisi say.
        return (datetime.utcnow() - self.last_seen).total_seconds() < 95


class Card(Base):
    __tablename__ = "cards"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"))
    uid: Mapped[str] = mapped_column(String(24), index=True)      # D3BF4E07
    name: Mapped[str] = mapped_column(String(32))
    is_admin: Mapped[bool] = mapped_column(Boolean, default=False)
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    quota_kwh: Mapped[int] = mapped_column(Integer, default=0)    # 0 = sinirsiz
    used_kwh: Mapped[float] = mapped_column(Float, default=0.0)
    last_used_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow)

    device: Mapped[Device] = relationship(back_populates="cards")

    @property
    def remaining(self) -> float | None:
        if self.quota_kwh == 0:
            return None
        return round(max(0.0, self.quota_kwh - self.used_kwh), 2)

    @property
    def pct(self) -> int:
        if self.quota_kwh == 0:
            return 0
        return int(min(100, self.used_kwh / self.quota_kwh * 100))


class Setting(Base):
    __tablename__ = "settings"

    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"),
                                           primary_key=True)
    max_a: Mapped[int] = mapped_column(Integer, default=16)
    def_a: Mapped[int] = mapped_column(Integer, default=10)
    require_card: Mapped[bool] = mapped_column(Boolean, default=True)
    schedule_en: Mapped[bool] = mapped_column(Boolean, default=False)
    schedule_start: Mapped[str] = mapped_column(String(5), default="22:00")
    schedule_end: Mapped[str] = mapped_column(String(5), default="06:00")
    schedule_days: Mapped[int] = mapped_column(Integer, default=127)   # bit0=Pzt
    max_session_min: Mapped[int] = mapped_column(Integer, default=480)
    target_kwh: Mapped[float] = mapped_column(Float, default=0.0)   # 0 = sinirsiz
    # Uzaktan komut: '' yok | 'start' | 'stop'. cmd_id tekillestirir,
    # ESP32 ayni id'yi iki kez uygulamaz.
    cmd: Mapped[str] = mapped_column(String(12), default="")
    cmd_id: Mapped[int] = mapped_column(Integer, default=0)
    rev: Mapped[int] = mapped_column(Integer, default=1)
    updated_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow,
                                                 onupdate=datetime.utcnow)

    device: Mapped[Device] = relationship(back_populates="settings")


class Session_(Base):
    __tablename__ = "sessions"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"))
    card_id: Mapped[int | None] = mapped_column(ForeignKey("cards.id",
                                                           ondelete="SET NULL"),
                                                nullable=True)
    uid: Mapped[str] = mapped_column(String(24), index=True)
    name: Mapped[str] = mapped_column(String(32), default="")
    started_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    ended_at: Mapped[datetime | None] = mapped_column(DateTime, nullable=True)
    kwh: Mapped[float] = mapped_column(Float, default=0.0)
    reason: Mapped[int] = mapped_column(Integer, default=0)
    # ESP32'nin urettigi tekil kimlik: internet kesintisinde kuyruktan
    # gonderilen kayitlar tekrar denendiginde mukerrer yazilmasin.
    client_id: Mapped[str] = mapped_column(String(48), default="", index=True)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow,
                                                 index=True)

    REASONS = {0: "normal", 4: "iptal", 5: "taninmayan kart", 6: "elektrik kesintisi"}


class Event(Base):
    __tablename__ = "events"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[int | None] = mapped_column(ForeignKey("devices.id",
                                                             ondelete="SET NULL"),
                                                  nullable=True)
    kind: Mapped[str] = mapped_column(String(32))
    detail: Mapped[str] = mapped_column(Text, default="")
    created_at: Mapped[datetime] = mapped_column(DateTime, default=datetime.utcnow,
                                                 index=True)


def init_db() -> None:
    Base.metadata.create_all(engine)


def get_db():
    db: Session = SessionLocal()
    try:
        yield db
    finally:
        db.close()
