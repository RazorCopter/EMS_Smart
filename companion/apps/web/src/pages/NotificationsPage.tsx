import { FormEvent, useEffect, useState } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { Bell, CheckCircle2, Send, Sparkles, Wand2 } from "lucide-react";
import { apiFetch, type AuthUser } from "../lib/api";
import { PageHeader } from "../components/shared/PageHeader";

interface Props {
  user: AuthUser;
}

interface CatalogItem {
  key: string;
  label: string;
  description: string;
  severity: "critical" | "error" | "warning" | "info";
  default_enabled: boolean;
  event_types: string[];
}

interface DiscoveredChat {
  chat_id: string;
  title: string;
  username?: string | null;
  type: string;
}

const SEVERITY_BADGE: Record<string, { label: string; cls: string }> = {
  critical: { label: "Critico", cls: "em-tone-err" },
  error: { label: "Errore", cls: "em-tone-err" },
  warning: { label: "Avviso", cls: "em-tone-warn" },
  info: { label: "Info", cls: "em-tone-ok" },
};

export function NotificationsPage({ user }: Props) {
  const qc = useQueryClient();
  const isAdmin = user.role === "admin";

  const { data, isLoading } = useQuery({
    queryKey: ["notifications-settings"],
    queryFn: () => apiFetch<any>("/api/v1/notifications/settings"),
  });

  const [enabled, setEnabled] = useState(false);
  const [botToken, setBotToken] = useState("");
  const [chatId, setChatId] = useState("");
  const [chatTitle, setChatTitle] = useState("");
  const [events, setEvents] = useState<Record<string, boolean>>({});
  const [wizardOpen, setWizardOpen] = useState(false);
  const [wizardStep, setWizardStep] = useState<1 | 2 | 3>(1);
  const [discoveredChats, setDiscoveredChats] = useState<DiscoveredChat[]>([]);
  const [botUsername, setBotUsername] = useState<string | null>(null);
  const [feedback, setFeedback] = useState<string | null>(null);
  const [testFeedback, setTestFeedback] = useState<string | null>(null);

  useEffect(() => {
    if (!data) return;
    setEnabled(Boolean(data.enabled));
    setChatId(String(data.chat_id ?? ""));
    setChatTitle(String(data.chat_title ?? ""));
    setEvents(data.events ?? {});
    if (!data.bot_token_configured || !data.chat_id) {
      setWizardOpen(true);
    }
  }, [data]);

  const saveMutation = useMutation({
    mutationFn: (payload: Record<string, unknown>) =>
      apiFetch<any>("/api/v1/notifications/settings", {
        method: "PUT",
        body: JSON.stringify(payload),
      }),
    onSuccess: (res) => {
      qc.invalidateQueries({ queryKey: ["notifications-settings"] });
      setBotToken("");
      setFeedback("Configurazione notifiche Telegram salvata.");
      if (res.bot_token_configured && res.chat_id) {
        setWizardOpen(false);
      }
    },
    onError: (err) => {
      setFeedback(err instanceof Error ? err.message : "Errore salvataggio notifiche");
    },
  });

  const discoverMutation = useMutation({
    mutationFn: (token: string) =>
      apiFetch<any>("/api/v1/notifications/telegram/discover", {
        method: "POST",
        body: JSON.stringify({ bot_token: token || null }),
      }),
    onSuccess: (res) => {
      if (!res.ok) {
        setTestFeedback(res.error || "Impossibile contattare Telegram.");
        setDiscoveredChats([]);
        return;
      }
      setBotUsername(res.bot?.username ?? null);
      setDiscoveredChats(res.chats ?? []);
      if ((res.chats ?? []).length === 0) {
        setTestFeedback(
          `Bot @${res.bot?.username ?? "bot"} valido, ma nessuna chat trovata. Apri il bot su Telegram, premi Avvia (/start) e riprova.`
        );
      } else {
        setTestFeedback(`Trovate ${res.chats.length} chat collegate a @${res.bot?.username ?? "bot"}. Seleziona la chat qui sotto.`);
      }
    },
    onError: (err) => {
      setTestFeedback(err instanceof Error ? err.message : "Errore rilevamento chat");
    },
  });

  const testMutation = useMutation({
    mutationFn: () =>
      apiFetch<any>("/api/v1/notifications/telegram/test", {
        method: "POST",
        body: JSON.stringify({
          bot_token: botToken.trim() || null,
          chat_id: chatId.trim() || null,
        }),
      }),
    onSuccess: (res) => {
      setTestFeedback(
        res.ok
          ? "Messaggio di test inviato su Telegram!"
          : `Invio fallito: ${res.detail}`
      );
    },
    onError: (err) => {
      setTestFeedback(err instanceof Error ? err.message : "Errore invio test");
    },
  });

  function handleSaveAll(forceEnable?: boolean) {
    setFeedback(null);
    saveMutation.mutate({
      enabled: forceEnable ?? enabled,
      bot_token: botToken.trim() || null,
      chat_id: chatId.trim(),
      chat_title: chatTitle.trim(),
      events,
    });
  }

  function onEventsSubmit(e: FormEvent) {
    e.preventDefault();
    handleSaveAll();
  }

  const catalog: CatalogItem[] = data?.catalog ?? [];
  const configured = Boolean(data?.bot_token_configured && (chatId || data?.chat_id));

  if (isLoading) {
    return <div className="card">Caricamento impostazioni notifiche...</div>;
  }

  return (
    <>
      <PageHeader
        title="Notifiche Telegram"
        subtitle="Avvisi immediati per cambio modalità inatteso, errori ESP32 e failsafe"
        actions={
          <button
            type="button"
            className="btn"
            onClick={() => {
              setWizardOpen((v) => !v);
              setWizardStep(1);
            }}
          >
            <Wand2 size={15} />
            <span>{wizardOpen ? "Chiudi Wizard" : "Wizard Telegram"}</span>
          </button>
        }
      />

      {feedback && (
        <div className="cyber-banner-feedback" style={{ borderColor: "var(--battery-color)" }}>
          {feedback}
        </div>
      )}

      {/* STATO CANALE TELEGRAM */}
      <section className="card cyber-card-gap">
        <div className="cyber-row-between">
          <div>
            <h3 className="card-title" style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <Bell size={18} />
              <span>Canale Telegram</span>
            </h3>
            <p className="muted" style={{ marginTop: 4 }}>
              {configured
                ? `Bot configurato (${data?.bot_token_masked}) · Chat: ${chatTitle || chatId}`
                : "Completa la wizard in 3 passaggi per collegare il tuo bot Telegram."}
            </p>
          </div>
          <label className={`cyber-switch ${!isAdmin || saveMutation.isPending ? "disabled" : ""}`}>
            <input
              type="checkbox"
              checked={enabled}
              disabled={!isAdmin || saveMutation.isPending}
              onChange={(e) => {
                const next = e.target.checked;
                setEnabled(next);
                if (configured) handleSaveAll(next);
              }}
            />
            <span className="cyber-switch-track" aria-hidden="true">
              <span className="cyber-switch-thumb" />
            </span>
            <strong>{enabled ? "Notifiche attive" : "Notifiche disattivate"}</strong>
          </label>
        </div>
      </section>

      {/* WIZARD GUIDATA 3 STEP */}
      {wizardOpen && (
        <section className="card cyber-card-gap">
          <div className="cyber-row-between" style={{ marginBottom: 12 }}>
            <h3 className="card-title" style={{ display: "flex", alignItems: "center", gap: 8 }}>
              <Sparkles size={17} />
              <span>Wizard configurazione Telegram</span>
            </h3>
            <div className="cyber-wizard-steps">
              {([1, 2, 3] as const).map((s) => (
                <button
                  key={s}
                  type="button"
                  className={wizardStep === s ? "btn btn-primary" : "btn"}
                  onClick={() => setWizardStep(s)}
                >
                  Step {s}
                </button>
              ))}
            </div>
          </div>

          {testFeedback && (
            <div className="cyber-banner-feedback">
              {testFeedback}
            </div>
          )}

          {wizardStep === 1 && (
            <div style={{ display: "grid", gap: 12 }}>
              <p className="muted">
                <strong>1. Crea il tuo bot:</strong> apri <code>@BotFather</code> su Telegram, invia <code>/newbot</code>, scegli un nome e incolla qui il <strong>Bot Token</strong>.
              </p>
              <label style={{ display: "grid", gap: 6 }}>
                <span>Bot Token Telegram</span>
                <input
                  type="password"
                  className="input"
                  value={botToken}
                  disabled={!isAdmin}
                  onChange={(e) => setBotToken(e.target.value)}
                  placeholder={data?.bot_token_configured ? `Salvato (${data.bot_token_masked}) — lascia vuoto per mantenere` : "es. 123456789:AAH..."}
                />
              </label>
              <div style={{ display: "flex", gap: 8 }}>
                <button type="button" className="btn btn-primary" onClick={() => setWizardStep(2)}>
                  Avanti · Step 2
                </button>
              </div>
            </div>
          )}

          {wizardStep === 2 && (
            <div style={{ display: "grid", gap: 12 }}>
              <p className="muted">
                <strong>2. Collega la chat:</strong> apri il bot su Telegram e premi <strong>Avvia</strong> (<code>/start</code>), poi clicca <strong>Rileva Chat ID</strong>.
              </p>
              <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
                <button
                  type="button"
                  className="btn"
                  disabled={!isAdmin || discoverMutation.isPending}
                  onClick={() => discoverMutation.mutate(botToken.trim())}
                >
                  {discoverMutation.isPending ? "Rilevamento..." : "Rileva Chat ID"}
                </button>
                {botUsername && <span className="muted">Bot: @{botUsername}</span>}
              </div>

              {discoveredChats.length > 0 && (
                <div style={{ display: "grid", gap: 8 }}>
                  {discoveredChats.map((c) => (
                    <button
                      key={c.chat_id}
                      type="button"
                      className={chatId === c.chat_id ? "btn btn-primary" : "btn"}
                      style={{ justifyContent: "space-between", textAlign: "left" }}
                      onClick={() => {
                        setChatId(c.chat_id);
                        setChatTitle(c.title);
                      }}
                    >
                      <span>
                        {c.title} {c.username ? `(@${c.username})` : ""} · [{c.type}]
                      </span>
                      <span className="mono">{c.chat_id}</span>
                    </button>
                  ))}
                </div>
              )}

              <label style={{ display: "grid", gap: 6 }}>
                <span>Chat ID (rilevato o manuale)</span>
                <input
                  type="text"
                  className="input mono"
                  value={chatId}
                  disabled={!isAdmin}
                  onChange={(e) => setChatId(e.target.value)}
                  placeholder="es. 123456789 o -100..."
                />
              </label>

              <div style={{ display: "flex", gap: 8 }}>
                <button type="button" className="btn" onClick={() => setWizardStep(1)}>
                  Indietro
                </button>
                <button type="button" className="btn btn-primary" onClick={() => setWizardStep(3)}>
                  Avanti · Step 3
                </button>
              </div>
            </div>
          )}

          {wizardStep === 3 && (
            <div style={{ display: "grid", gap: 12 }}>
              <p className="muted">
                <strong>3. Verifica e salva:</strong> invia un messaggio di test per confermare la ricezione su Telegram, poi salva e attiva le notifiche.
              </p>
              <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
                <button type="button" className="btn" onClick={() => setWizardStep(2)}>
                  Indietro
                </button>
                <button
                  type="button"
                  className="btn"
                  disabled={!isAdmin || testMutation.isPending || !chatId.trim()}
                  onClick={() => testMutation.mutate()}
                >
                  <Send size={14} />
                  <span>{testMutation.isPending ? "Invio..." : "Invia messaggio di test"}</span>
                </button>
                <button
                  type="button"
                  className="btn btn-primary"
                  disabled={!isAdmin || saveMutation.isPending || !chatId.trim()}
                  onClick={() => {
                    setEnabled(true);
                    handleSaveAll(true);
                  }}
                >
                  <CheckCircle2 size={14} />
                  <span>{saveMutation.isPending ? "Salvataggio..." : "Salva e attiva notifiche"}</span>
                </button>
              </div>
            </div>
          )}
        </section>
      )}

      {/* LISTA EVENTI NOTIFICABILI */}
      <section className="card">
        <form onSubmit={onEventsSubmit} style={{ display: "grid", gap: 12 }}>
          <div>
            <h3 className="card-title">Eventi notificabili</h3>
            <p className="muted">Scegli quali eventi generano un avviso istantaneo su Telegram.</p>
          </div>

          <div className="cyber-stack">
            {catalog.map((item) => {
              const sev = SEVERITY_BADGE[item.severity] ?? SEVERITY_BADGE.info;
              const checked = Boolean(events[item.key]);
              return (
                <label
                  key={item.key}
                  className="cyber-notif-row"
                >
                  <div style={{ display: "grid", gap: 4 }}>
                    <div style={{ display: "flex", alignItems: "center", gap: 8, flexWrap: "wrap" }}>
                      <strong>{item.label}</strong>
                      <span className={`em-status-pill ${sev.cls}`} style={{ fontSize: 11, padding: "2px 8px" }}>
                        {sev.label}
                      </span>
                    </div>
                    <span className="muted" style={{ fontSize: 13 }}>{item.description}</span>
                  </div>
                  <span className={`cyber-switch ${!isAdmin ? "disabled" : ""}`}>
                    <input
                      type="checkbox"
                      checked={checked}
                      disabled={!isAdmin}
                      onChange={(e) =>
                        setEvents((prev) => ({ ...prev, [item.key]: e.target.checked }))
                      }
                    />
                    <span className="cyber-switch-track" aria-hidden="true">
                      <span className="cyber-switch-thumb" />
                    </span>
                  </span>
                </label>
              );
            })}
          </div>

          <div style={{ display: "flex", gap: 10, justifyContent: "flex-end" }}>
            {configured && (
              <button
                type="button"
                className="btn"
                disabled={!isAdmin || testMutation.isPending}
                onClick={() => testMutation.mutate()}
              >
                <Send size={14} />
                <span>Invia test</span>
              </button>
            )}
            <button type="submit" className="btn btn-primary" disabled={!isAdmin || saveMutation.isPending}>
              {saveMutation.isPending ? "Salvataggio..." : "Salva preferenze eventi"}
            </button>
          </div>
        </form>
      </section>
    </>
  );
}
