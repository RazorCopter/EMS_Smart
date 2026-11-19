from __future__ import annotations

import hashlib
import hmac
import logging
import os
import re
from typing import Any

SECRET_KEYS = ("password", "secret", "token", "api_key", "authorization", "cookie")
_SECRET_PATTERN = re.compile(
    r'(?i)("?(?:password|secret|token|api_key|authorization)"?\s*[:=]\s*"?)([^"\s,}]+)'
)


def redact_dict(data: Any) -> Any:
    """Recursively masks secret values in dictionaries/lists."""
    if isinstance(data, dict):
        out: dict[str, Any] = {}
        for k, v in data.items():
            if any(sk in str(k).lower() for sk in SECRET_KEYS):
                out[k] = "***REDACTED***" if v else ""
            else:
                out[k] = redact_dict(v)
        return out
    if isinstance(data, list):
        return [redact_dict(item) for item in data]
    return data


class SecretRedactingFilter(logging.Filter):
    """Logging filter that strips credential-like key=value pairs from log messages."""

    def filter(self, record: logging.LogRecord) -> bool:
        msg = record.getMessage()
        redacted = _SECRET_PATTERN.sub(r"\1***REDACTED***", msg)
        if redacted != msg:
            record.msg = redacted
            record.args = ()
        return True


def hash_password(password: str, salt: bytes | None = None) -> str:
    salt_bytes = salt or os.urandom(16)
    dk = hashlib.scrypt(password.encode("utf-8"), salt=salt_bytes, n=16384, r=8, p=1, dklen=32)
    return f"scrypt${salt_bytes.hex()}${dk.hex()}"


def verify_password(password: str, stored_hash: str) -> bool:
    try:
        scheme, salt_hex, dk_hex = stored_hash.split("$", 2)
        if scheme != "scrypt":
            return False
        salt = bytes.fromhex(salt_hex)
        expected = bytes.fromhex(dk_hex)
        actual = hashlib.scrypt(password.encode("utf-8"), salt=salt, n=16384, r=8, p=1, dklen=32)
        return hmac.compare_digest(actual, expected)
    except Exception:
        return False
