# Sicurezza · Afore EMS Companion

## Misure implementate

1. **Sola lettura verso impianto**: nessuna credenziale Modbus/LIVE è presente nel Companion; il client REST ESP32 implementa esclusivamente richieste `GET`.
2. **Autenticazione locale**: password hashate con `scrypt` + salt casuale a 16 byte e confronto a tempo costante (`hmac.compare_digest`).
3. **Sessioni e CSRF**: cookie `HttpOnly` firmato `HMAC-SHA256`, `SameSite=Lax`, protezione CSRF su tutte le mutazioni Admin (`X-CSRF-Token`).
4. **Rate limiting**: limite di 10 tentativi di login/minuto per IP su `POST /api/v1/auth/login`.
5. **Redazione segreti**: nessuna password o token viene esposto dalle API (`/api/v1/settings` restituisce solo `*_configured: true/false`); `SecretRedactingFilter` maschera eventuali chiavi sensibili nei log.
6. **Ruoli**: `viewer` (consultazione ed esportazione) e `admin` (impostazioni Companion). Nessun ruolo può abilitare LIVE sull'ESP32.

## Evoluzione futura (fuori LAN fidata)

- Terminazione TLS 1.3 sul reverse proxy (Caddy/Nginx/Traefik) e impostazione `COMPANION_COOKIE_SECURE=true`.
- Integrazione opzionale OIDC / Microsoft Entra ID mappando i claim di gruppo sui ruoli `viewer` / `admin`.
