from __future__ import annotations

import base64
from collections import defaultdict, deque
from dataclasses import dataclass
import hashlib
import hmac
import json
import secrets
import time
from typing import Literal

from fastapi import Cookie, Depends, Header, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from companion_db.models import CompanionUser
from energy_domain import hash_password, verify_password

Role = Literal["viewer", "admin"]
SESSION_COOKIE_NAME = "afore_companion_session"
CSRF_COOKIE_NAME = "afore_companion_csrf"


@dataclass
class SessionInfo:
    username: str
    role: Role
    exp: int
    csrf_token: str


class LoginRateLimiter:
    """Sliding-window per-IP rate limiter for /api/v1/auth/login."""

    def __init__(self, max_attempts: int = 10, window_seconds: int = 60) -> None:
        self.max_attempts = max_attempts
        self.window_seconds = window_seconds
        self._hits: dict[str, deque[float]] = defaultdict(deque)

    def check(self, key: str) -> None:
        now = time.monotonic()
        bucket = self._hits[key]
        while bucket and now - bucket[0] > self.window_seconds:
            bucket.popleft()
        if len(bucket) >= self.max_attempts:
            raise HTTPException(
                status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                detail="Troppi tentativi di accesso. Riprova tra un minuto.",
            )
        bucket.append(now)


login_rate_limiter = LoginRateLimiter()


def sign_session(username: str, role: Role, secret_key: str, ttl_hours: int) -> tuple[str, str]:
    exp = int(time.time()) + ttl_hours * 3600
    csrf_token = secrets.token_urlsafe(24)
    payload = {"u": username, "r": role, "exp": exp, "csrf": csrf_token}
    encoded = base64.urlsafe_b64encode(json.dumps(payload, separators=(",", ":")).encode("utf-8")).decode("ascii")
    sig = hmac.new(secret_key.encode("utf-8"), encoded.encode("ascii"), hashlib.sha256).hexdigest()
    return f"{encoded}.{sig}", csrf_token


def verify_session(token: str | None, secret_key: str) -> SessionInfo | None:
    if not token or "." not in token:
        return None
    try:
        encoded, sig = token.rsplit(".", 1)
        expected = hmac.new(secret_key.encode("utf-8"), encoded.encode("ascii"), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(sig, expected):
            return None
        payload = json.loads(base64.urlsafe_b64decode(encoded.encode("ascii")).decode("utf-8"))
        if int(payload["exp"]) < int(time.time()):
            return None
        role = payload["r"]
        if role not in ("viewer", "admin"):
            return None
        return SessionInfo(
            username=str(payload["u"]),
            role=role,
            exp=int(payload["exp"]),
            csrf_token=str(payload["csrf"]),
        )
    except Exception:
        return None


async def ensure_bootstrap_users(
    session: AsyncSession,
    *,
    admin_user: str,
    admin_password: str,
    viewer_user: str,
    viewer_password: str,
) -> None:
    for uname, pwd, role in [
        (admin_user, admin_password, "admin"),
        (viewer_user, viewer_password, "viewer"),
    ]:
        if not uname or not pwd:
            continue
        existing = (
            await session.execute(select(CompanionUser).where(CompanionUser.username == uname))
        ).scalar_one_or_none()
        if existing is None:
            session.add(
                CompanionUser(
                    username=uname,
                    password_hash=hash_password(pwd),
                    role=role,
                    enabled=True,
                )
            )
    await session.commit()


async def authenticate_user(session: AsyncSession, username: str, password: str) -> CompanionUser | None:
    user = (
        await session.execute(select(CompanionUser).where(CompanionUser.username == username))
    ).scalar_one_or_none()
    if user is None or not user.enabled:
        return None
    if not verify_password(password, user.password_hash):
        return None
    return user


def require_viewer(
    request: Request,
    session_cookie: str | None = Cookie(default=None, alias=SESSION_COOKIE_NAME),
) -> SessionInfo:
    secret_key: str = request.app.state.settings.companion_secret_key
    info = verify_session(session_cookie, secret_key)
    if info is None:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Autenticazione richiesta")
    return info


def require_viewer_csrf(
    request: Request,
    info: SessionInfo = Depends(require_viewer),
    x_csrf_token: str | None = Header(default=None, alias="X-CSRF-Token"),
) -> SessionInfo:
    if request.method in {"POST", "PUT", "PATCH", "DELETE"}:
        if not x_csrf_token or not hmac.compare_digest(x_csrf_token, info.csrf_token):
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Token CSRF mancante o non valido")
    return info


def require_admin_csrf(
    info: SessionInfo = Depends(require_viewer_csrf),
) -> SessionInfo:
    if info.role != "admin":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Richiesti privilegi Admin")
    return info
