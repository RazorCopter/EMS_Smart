interface Props {
  espOnline: boolean;
  rs485Active: boolean;
  relayActive: boolean;
  liveControl: boolean;
}

export function WaveshareEsp32DinSvg({ espOnline, rs485Active, relayActive, liveControl }: Props) {
  const rs485Class = espOnline && rs485Active ? "em-led-rs485" : "em-led-off";
  const relayClass = !espOnline
    ? "em-led-off"
    : relayActive
    ? "em-led-relay-on"
    : liveControl
    ? "em-led-relay-live"
    : "em-led-off";
  const pwrClass = espOnline ? "em-led-pwr-on" : "em-led-off";

  return (
    <div className="em-esp-din-wrap" title="Waveshare ESP32-S3-Relay-1CH">
      <svg
        viewBox="0 0 110 260"
        className="em-esp-din-svg"
        role="img"
        aria-label={`Modulo Waveshare ESP32-S3-Relay-1CH (${espOnline ? "Online" : "Offline"})`}
      >
        <defs>
          <filter id="em-led-glow" x="-60%" y="-60%" width="220%" height="220%">
            <feGaussianBlur stdDeviation="2.4" result="blur" />
            <feMerge>
              <feMergeNode in="blur" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
          <linearGradient id="em-din-body" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0%" stopColor="#d8e1ec" />
            <stop offset="50%" stopColor="#eef3f8" />
            <stop offset="100%" stopColor="#cfd9e6" />
          </linearGradient>
        </defs>

        {/* Top orange DIN rail clip */}
        <path
          d="M 41 12 L 45 3 L 65 3 L 69 12 Z"
          fill="#ff4d1a"
          stroke="#c2410c"
          strokeWidth="0.8"
        />
        <circle cx="55" cy="8.5" r="2.6" fill="#0b111a" />

        {/* Upper terminal housing */}
        <rect x="20" y="12" width="70" height="32" rx="4" fill="url(#em-din-body)" stroke="rgba(0, 240, 255, 0.35)" strokeWidth="1" />
        {/* 4 top screw terminals */}
        {[31, 47, 63, 79].map((cx) => (
          <g key={`top-${cx}`}>
            <circle cx={cx} cy="26" r="5.5" fill="#1e293b" stroke="#64748b" strokeWidth="0.8" />
            <circle cx={cx} cy="26" r="4" fill="#94a3b8" />
            <line x1={cx - 2.6} y1="23.4" x2={cx + 2.6} y2="28.6" stroke="#334155" strokeWidth="1.2" />
          </g>
        ))}

        {/* Main stepped DIN body */}
        <rect x="17" y="42" width="76" height="174" rx="5" fill="url(#em-din-body)" stroke="rgba(0, 240, 255, 0.45)" strokeWidth="1.1" />

        {/* Lower terminal housing */}
        <rect x="20" y="214" width="70" height="32" rx="4" fill="url(#em-din-body)" stroke="rgba(0, 240, 255, 0.35)" strokeWidth="1" />
        {/* 4 bottom screw terminals */}
        {[31, 47, 63, 79].map((cx) => (
          <g key={`bot-${cx}`}>
            <circle cx={cx} cy="230" r="5.5" fill="#1e293b" stroke="#64748b" strokeWidth="0.8" />
            <circle cx={cx} cy="230" r="4" fill="#94a3b8" />
            <line x1={cx - 2.6} y1="227.4" x2={cx + 2.6} y2="232.6" stroke="#334155" strokeWidth="1.2" />
          </g>
        ))}

        {/* Black Waveshare faceplate */}
        <rect x="24" y="54" width="62" height="150" rx="3" fill="#0d131a" stroke="#1e293b" strokeWidth="1" />

        {/* USB-C green port & GND/VCC dots */}
        <rect x="29" y="59" width="22" height="9" rx="4.5" fill="#4d7c0f" stroke="#84cc16" strokeWidth="0.8" />
        <rect x="34" y="62.2" width="12" height="2.6" rx="1.3" fill="#090d12" />
        <circle cx="62" cy="64" r="5.2" fill="#4d7c0f" stroke="#84cc16" strokeWidth="0.7" />
        <text x="62" y="65.8" textAnchor="middle" fill="#ecfccb" fontSize="4.2" fontWeight="700">GND</text>
        <circle cx="76" cy="64" r="5.2" fill="#4d7c0f" stroke="#84cc16" strokeWidth="0.7" />
        <text x="76" y="65.8" textAnchor="middle" fill="#ecfccb" fontSize="4.2" fontWeight="700">VCC</text>

        <text x="69" y="76" textAnchor="middle" fill="#cbd5e1" fontSize="5.5" fontWeight="700">DC 5V</text>

        {/* 3 Status LEDs: RS485, Relay, PWR */}
        <g>
          <circle cx="35" cy="91" r="4.2" className={rs485Class} filter={espOnline && rs485Active ? "url(#em-led-glow)" : undefined} />
          <text x="35" y="102" textAnchor="middle" fill="#94a3b8" fontSize="4.6" fontWeight="600">RS485</text>

          <circle cx="55" cy="91" r="4.2" className={relayClass} filter={espOnline && (relayActive || liveControl) ? "url(#em-led-glow)" : undefined} />
          <text x="55" y="102" textAnchor="middle" fill="#94a3b8" fontSize="4.6" fontWeight="600">Relay</text>

          <circle cx="75" cy="91" r="4.2" className={pwrClass} filter={espOnline ? "url(#em-led-glow)" : undefined} />
          <text x="75" y="102" textAnchor="middle" fill="#94a3b8" fontSize="4.6" fontWeight="600">PWR</text>
        </g>

        {/* Center vertical branding + green arrows */}
        <line x1="53" y1="112" x2="53" y2="162" stroke="#65a30d" strokeWidth="1.2" />
        <line x1="57" y1="112" x2="57" y2="162" stroke="#65a30d" strokeWidth="1.2" />
        <text
          x="44"
          y="138"
          textAnchor="middle"
          fill="#cbd5e1"
          fontSize="5.3"
          fontWeight="700"
          transform="rotate(90 44 138)"
        >
          ESP32-S3-Relay-1CH
        </text>
        <text
          x="67"
          y="138"
          textAnchor="middle"
          fill="#e2e8f0"
          fontSize="5.6"
          fontWeight="800"
          letterSpacing="0.06em"
          transform="rotate(90 67 138)"
        >
          WAVESHARE
        </text>

        {/* Bottom Relay / RS485 labels & green terminal badges */}
        <text x="40" y="177" textAnchor="middle" fill="#cbd5e1" fontSize="5.5" fontWeight="700">Relay</text>
        <text x="69" y="177" textAnchor="middle" fill="#cbd5e1" fontSize="5.5" fontWeight="700">RS485</text>

        {[
          { x: 32, lbl: "NO" },
          { x: 47, lbl: "COM" },
          { x: 63, lbl: "B-" },
          { x: 78, lbl: "A+" },
        ].map((t) => (
          <g key={t.lbl}>
            <circle cx={t.x} cy="193" r="5.5" fill="#4d7c0f" stroke="#84cc16" strokeWidth="0.7" />
            <text x={t.x} y="194.8" textAnchor="middle" fill="#ecfccb" fontSize="4.1" fontWeight="800">
              {t.lbl}
            </text>
          </g>
        ))}
      </svg>
    </div>
  );
}
