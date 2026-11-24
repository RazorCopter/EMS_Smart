from __future__ import annotations

from datetime import datetime, timezone
from html import escape
from typing import Any
from zoneinfo import ZoneInfo

NOTIFICATION_SETTINGS_KEY = "notifications_telegram"

NOTIFICATION_CATALOG: list[dict[str, Any]] = [
    {
        "key": "mode_fallback_shadow",
        "label": "Cambio modalità inatteso (LIVE → SHADOW)",
        "description": "Avvisa subito quando l'ESP32 esce da LIVE e torna in SHADOW (es. mismatch trainer, fault o rilascio).",
        "severity": "critical",
        "default_enabled": True,
        "event_types": ["MODE_FALLBACK_SHADOW"],
    },
    {
        "key": "mode_changed_live",
        "label": "Attivazione modalità LIVE",
        "description": "Notifica quando il controllo reale LIVE viene avviato sull'ESP32.",
        "severity": "info",
        "default_enabled": True,
        "event_types": ["MODE_LIVE_ACTIVE"],
    },
    {
        "key": "esp32_error_failsafe",
        "label": "Errori ESP32, Fault inverter e Failsafe",
        "description": "Avvisa in caso di fault inverter, telemetria scaduta (SAFE_MODE), abort manutenzione o limiti sicurezza.",
        "severity": "error",
        "default_enabled": True,
        "event_types": ["INVERTER_FAULT", "TELEMETRY_STALE", "MAINTENANCE_FAULT_ABORT", "POWER_LIMIT"],
    },
    {
        "key": "esp32_offline",
        "label": "ESP32 Offline / Riavvio",
        "description": "Notifica quando l'ESP32 si disconnette da MQTT, torna online o si riavvia.",
        "severity": "warning",
        "default_enabled": True,
        "event_types": ["MQTT_OFFLINE", "MQTT_ONLINE", "ESP_REBOOT"],
    },
    {
        "key": "ha_offline",
        "label": "Home Assistant Offline / Online",
        "description": "Notifica la perdita o il ripristino dell'heartbeat di Home Assistant.",
        "severity": "warning",
        "default_enabled": True,
        "event_types": ["HA_OFFLINE", "HA_ONLINE"],
    },
    {
        "key": "target_unreachable",
        "label": "Target SOC irraggiungibile",
        "description": "Avvisa se il target SOC giornaliero non è raggiungibile con i limiti di potenza attuali.",
        "severity": "warning",
        "default_enabled": False,
        "event_types": ["TARGET_UNREACHABLE"],
    },
    {
        "key": "soc_limit",
        "label": "Interventi Safety SOC (Riserva / Ceiling)",
        "description": "Notifica quando viene raggiunta la riserva minima o il limite superiore di carica SOC.",
        "severity": "info",
        "default_enabled": False,
        "event_types": ["SOC_LIMIT"],
    },
    {
        "key": "strategy_change",
        "label": "Cambio strategia EMS",
        "description": "Notifica ogni cambio di strategia automatica (es. SELF_USE, CHARGE_FIRST, TARGET_SOC).",
        "severity": "info",
        "default_enabled": False,
        "event_types": ["STRATEGY_CHANGE"],
    },
]

DEFAULT_NOTIFICATION_EVENTS: dict[str, bool] = {
    item["key"]: bool(item["default_enabled"]) for item in NOTIFICATION_CATALOG
}

EVENT_TYPE_TO_NOTIFICATION_KEY: dict[str, str] = {
    evt: item["key"]
    for item in NOTIFICATION_CATALOG
    for evt in item["event_types"]
}


def normalize_notification_settings(raw: dict[str, Any] | None) -> dict[str, Any]:
    data = raw if isinstance(raw, dict) else {}
    raw_events = data.get("events") if isinstance(data.get("events"), dict) else {}
    events = {
        key: bool(raw_events.get(key, default_val))
        for key, default_val in DEFAULT_NOTIFICATION_EVENTS.items()
    }
    return {
        "enabled": bool(data.get("enabled", False)),
        "bot_token": str(data.get("bot_token") or "").strip(),
        "chat_id": str(data.get("chat_id") or "").strip(),
        "chat_title": str(data.get("chat_title") or "").strip(),
        "events": events,
    }


def mask_bot_token(token: str | None) -> str:
    t = (token or "").strip()
    if not t:
        return ""
    if len(t) <= 10:
        return "*" * len(t)
    return f"{t[:6]}...{t[-4:]}"


def format_telegram_event_message(
    *,
    event_type: str,
    severity: str,
    title: str,
    detail: str | None = None,
    occurred_at: datetime | None = None,
    tz_name: str = "Europe/Rome",
) -> str:
    icon = {
        "critical": "🚨",
        "error": "❌",
        "warning": "⚠️",
        "info": "ℹ️",
    }.get(severity.lower(), "🔔")
    if event_type == "MODE_LIVE_ACTIVE":
        icon = "✅"
    ts = occurred_at or datetime.now(timezone.utc)
    if ts.tzinfo is None:
        ts = ts.replace(tzinfo=timezone.utc)
    try:
        local_ts = ts.astimezone(ZoneInfo(tz_name))
    except Exception:
        local_ts = ts
    ts_str = local_ts.strftime("%d/%m/%Y %H:%M:%S")
    lines = [
        f"{icon} <b>Afore EMS · {escape(title)}</b>",
        f"<b>Evento:</b> <code>{escape(event_type)}</code>",
    ]
    if detail:
        lines.append(f"<b>Dettaglio:</b> {escape(detail)}")
    lines.append(f"<b>Ora:</b> {escape(ts_str)}")
    return "\n".join(lines)


async def send_telegram_message(
    bot_token: str,
    chat_id: str,
    text: str,
    *,
    timeout_s: float = 5.0,
) -> tuple[bool, str]:
    import httpx

    token = bot_token.strip()
    cid = chat_id.strip()
    if not token or not cid:
        return False, "Bot token o Chat ID mancanti"
    url = f"https://api.telegram.org/bot{token}/sendMessage"
    try:
        async with httpx.AsyncClient(timeout=timeout_s) as client:
            resp = await client.post(
                url,
                json={
                    "chat_id": cid,
                    "text": text,
                    "parse_mode": "HTML",
                    "disable_web_page_preview": True,
                },
            )
        payload = resp.json()
        if resp.status_code == 200 and payload.get("ok"):
            return True, "Messaggio inviato"
        return False, str(payload.get("description") or f"HTTP {resp.status_code}")
    except Exception as exc:
        return False, str(exc)[:180]


async def discover_telegram_chats(bot_token: str, *, timeout_s: float = 6.0) -> dict[str, Any]:
    import httpx

    token = bot_token.strip()
    if not token:
        return {"ok": False, "error": "Inserisci il Bot Token prima di rilevare la chat.", "chats": []}
    try:
        async with httpx.AsyncClient(timeout=timeout_s) as client:
            me_resp = await client.get(f"https://api.telegram.org/bot{token}/getMe")
            me_json = me_resp.json()
            if me_resp.status_code != 200 or not me_json.get("ok"):
                return {
                    "ok": False,
                    "error": str(me_json.get("description") or f"Token non valido (HTTP {me_resp.status_code})"),
                    "chats": [],
                }
            upd_resp = await client.get(
                f"https://api.telegram.org/bot{token}/getUpdates",
                params={"limit": 25, "allowed_updates": '["message","my_chat_member","channel_post"]'},
            )
            upd_json = upd_resp.json()
            if upd_resp.status_code != 200 or not upd_json.get("ok"):
                return {
                    "ok": False,
                    "bot": me_json.get("result"),
                    "error": str(upd_json.get("description") or f"Errore getUpdates (HTTP {upd_resp.status_code})"),
                    "chats": [],
                }
        seen: dict[str, dict[str, Any]] = {}
        for upd in upd_json.get("result", []):
            msg = upd.get("message") or upd.get("channel_post") or upd.get("my_chat_member") or {}
            chat = msg.get("chat")
            if not isinstance(chat, dict) or "id" not in chat:
                continue
            cid = str(chat["id"])
            title = (
                chat.get("title")
                or " ".join(x for x in [chat.get("first_name"), chat.get("last_name")] if x).strip()
                or chat.get("username")
                or cid
            )
            seen[cid] = {
                "chat_id": cid,
                "title": title,
                "username": chat.get("username"),
                "type": chat.get("type", "private"),
            }
        return {
            "ok": True,
            "bot": me_json.get("result"),
            "chats": list(seen.values()),
        }
    except Exception as exc:
        return {"ok": False, "error": str(exc)[:180], "chats": []}
