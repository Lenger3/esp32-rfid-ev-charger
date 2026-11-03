import hashlib
import hmac
import os
import secrets
from datetime import datetime, timedelta

from fastapi import Cookie, HTTPException, Request, status

ADMIN_PASSWORD = os.getenv("ADMIN_PASSWORD", "")
SESSION_SECRET = os.getenv("SESSION_SECRET", "")
COOKIE_NAME = "ev_session"
SESSION_HOURS = 12


def _secret() -> bytes:
    if not SESSION_SECRET:
        raise RuntimeError("SESSION_SECRET tanimli degil")
    return SESSION_SECRET.encode()


def check_password(pw: str) -> bool:
    if not ADMIN_PASSWORD:
        return False
    return hmac.compare_digest(pw, ADMIN_PASSWORD)


def make_token() -> str:
    """imza.zaman — HMAC ile, sunucuda oturum tutmadan."""
    exp = int((datetime.utcnow() + timedelta(hours=SESSION_HOURS)).timestamp())
    nonce = secrets.token_hex(8)
    payload = f"{exp}.{nonce}"
    sig = hmac.new(_secret(), payload.encode(), hashlib.sha256).hexdigest()[:32]
    return f"{payload}.{sig}"


def verify_token(token: str | None) -> bool:
    if not token:
        return False
    try:
        exp_s, nonce, sig = token.split(".")
        payload = f"{exp_s}.{nonce}"
        want = hmac.new(_secret(), payload.encode(), hashlib.sha256).hexdigest()[:32]
        if not hmac.compare_digest(sig, want):
            return False
        return int(exp_s) > int(datetime.utcnow().timestamp())
    except (ValueError, RuntimeError):
        return False


def require_login(ev_session: str | None = Cookie(default=None)):
    if not verify_token(ev_session):
        raise HTTPException(status_code=status.HTTP_303_SEE_OTHER,
                            headers={"Location": "/login"})
    return True


def require_device(request: Request, authorization: str | None = None):
    """ESP32 tarafi: Authorization: Bearer <token>"""
    auth = authorization or request.headers.get("authorization", "")
    if not auth.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="token yok")
    return auth[7:].strip()
