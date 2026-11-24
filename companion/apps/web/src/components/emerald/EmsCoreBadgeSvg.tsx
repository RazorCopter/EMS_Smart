export function EmsCoreBadgeSvg() {
  return (
    <svg
      viewBox="0 0 120 120"
      className="em-ems-badge-svg"
      role="img"
      aria-label="EMS Core"
    >
      <defs>
        <linearGradient id="ems-rim-grad" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0%" stopColor="#00f0ff" />
          <stop offset="55%" stopColor="#10f5a0" />
          <stop offset="100%" stopColor="#fbbf24" />
        </linearGradient>
        <linearGradient id="ems-bg-grad" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0%" stopColor="#081624" />
          <stop offset="100%" stopColor="#071d1b" />
        </linearGradient>
        <linearGradient id="ems-core-grad" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0%" stopColor="#0ea5e9" stopOpacity="0.35" />
          <stop offset="100%" stopColor="#10f5a0" stopOpacity="0.2" />
        </linearGradient>
        <filter id="ems-neon-glow" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="1.8" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      {/* Outer rounded-square cyber frame */}
      <rect x="4" y="4" width="112" height="112" rx="22" fill="url(#ems-bg-grad)" stroke="url(#ems-rim-grad)" strokeWidth="2.4" />
      <rect x="9" y="9" width="102" height="102" rx="18" fill="none" stroke="rgba(0, 240, 255, 0.28)" strokeWidth="1" />

      {/* Top-left & top-right neural nodes */}
      <g stroke="rgba(0, 240, 255, 0.5)" strokeWidth="1" fill="#00f0ff">
        <line x1="18" y1="20" x2="27" y2="17" />
        <line x1="18" y1="20" x2="21" y2="29" />
        <line x1="27" y1="17" x2="21" y2="29" />
        <circle cx="18" cy="20" r="1.6" />
        <circle cx="27" cy="17" r="1.6" />
        <circle cx="21" cy="29" r="1.6" />
      </g>
      <g stroke="rgba(16, 245, 160, 0.5)" strokeWidth="1" fill="#10f5a0">
        <line x1="102" y1="20" x2="93" y2="17" />
        <line x1="102" y1="20" x2="99" y2="29" />
        <line x1="93" y1="17" x2="99" y2="29" />
        <circle cx="102" cy="20" r="1.6" />
        <circle cx="93" cy="17" r="1.6" />
        <circle cx="99" cy="29" r="1.6" />
      </g>

      {/* 3-arrow orbital energy cycle */}
      <g className="em-ems-orbit" filter="url(#ems-neon-glow)">
        <path
          d="M 33 34 A 34 34 0 0 1 78 24"
          fill="none"
          stroke="#00f0ff"
          strokeWidth="3.5"
          strokeLinecap="round"
        />
        <polygon points="82,25 73,20 75,29" fill="#00f0ff" />

        <path
          d="M 88 38 A 34 34 0 0 1 82 82"
          fill="none"
          stroke="#10f5a0"
          strokeWidth="3.5"
          strokeLinecap="round"
        />
        <polygon points="79,86 87,81 80,76" fill="#10f5a0" />

        <path
          d="M 35 80 A 34 34 0 0 1 28 42"
          fill="none"
          stroke="#38bdf8"
          strokeWidth="3.5"
          strokeLinecap="round"
        />
        <polygon points="29,37 23,45 32,45" fill="#38bdf8" />
      </g>

      {/* Circuit traces radiating from central chip */}
      <g stroke="#38bdf8" strokeWidth="1.2" fill="none" opacity="0.75">
        <path d="M 42 50 L 34 50 L 30 46" />
        <path d="M 42 56 L 31 56" />
        <path d="M 42 62 L 34 62 L 30 66" />
        <path d="M 78 50 L 86 50 L 90 46" />
        <path d="M 78 56 L 89 56" />
        <path d="M 78 62 L 86 62 L 90 66" />
        <circle cx="30" cy="46" r="1.5" fill="#00f0ff" />
        <circle cx="31" cy="56" r="1.5" fill="#00f0ff" />
        <circle cx="90" cy="46" r="1.5" fill="#10f5a0" />
        <circle cx="89" cy="56" r="1.5" fill="#fbbf24" />
      </g>

      {/* Central Inverter/EMS Chip */}
      <rect
        x="43"
        y="37"
        width="34"
        height="38"
        rx="7"
        fill="url(#ems-core-grad)"
        stroke="url(#ems-rim-grad)"
        strokeWidth="1.8"
      />
      <circle cx="60" cy="56" r="10.5" fill="#07111a" stroke="#00f0ff" strokeWidth="1.3" />
      {/* AC/DC conversion glyph */}
      <path d="M 54.5 54 Q 56.5 51.5 58.5 54 T 62.5 54" fill="none" stroke="#10f5a0" strokeWidth="1.4" strokeLinecap="round" />
      <line x1="55" y1="60.5" x2="65" y2="60.5" stroke="#00f0ff" strokeWidth="1.4" strokeLinecap="round" />

      {/* Tiny lightning & leaf accents */}
      <polygon points="58,28 55,33 58,33 57,36 61,31 58,31" fill="#fbbf24" />
      <polygon points="90,92 87,97 90,97 89,100 93,95 90,95" fill="#fbbf24" />
      <path d="M 24 96 Q 24 89 31 89 Q 31 96 24 96 Z" fill="#10f5a0" opacity="0.9" />

      {/* EMS wordmark */}
      <text
        x="60"
        y="102"
        textAnchor="middle"
        fill="#e2f8ff"
        fontSize="16"
        fontWeight="900"
        letterSpacing="0.08em"
        filter="url(#ems-neon-glow)"
      >
        EMS
      </text>
    </svg>
  );
}
