from fastapi import HTTPException
import pytest

from companion_api.auth import LoginRateLimiter, sign_session, verify_session
from companion_api.pdf_export import build_simple_pdf


def test_session_signing_and_tamper_rejection():
    secret = "super-secret-test-key"
    token, csrf = sign_session("viewer", "viewer", secret, ttl_hours=1)
    assert len(csrf) > 10

    info = verify_session(token, secret)
    assert info is not None
    assert info.username == "viewer"
    assert info.role == "viewer"
    assert info.csrf_token == csrf

    assert verify_session(token + "x", secret) is None
    assert verify_session(token, "wrong-secret") is None


def test_login_rate_limiter_blocks_excessive_attempts():
    limiter = LoginRateLimiter(max_attempts=3, window_seconds=60)
    limiter.check("127.0.0.1")
    limiter.check("127.0.0.1")
    limiter.check("127.0.0.1")
    with pytest.raises(HTTPException) as exc:
        limiter.check("127.0.0.1")
    assert exc.value.status_code == 429


def test_pdf_generator_outputs_valid_pdf_header_and_eof():
    pdf = build_simple_pdf("Afore EMS Companion Test", ["Line 1", "SHADOW mode"])
    assert pdf.startswith(b"%PDF-1.4")
    assert b"%%EOF" in pdf
