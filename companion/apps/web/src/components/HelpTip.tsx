import { useState } from "react";
import { Info } from "lucide-react";

export function HelpTip({
  id,
  title,
  text,
  align = "left",
}: {
  id: string;
  title: string;
  text: string;
  align?: "left" | "right";
}) {
  const [open, setOpen] = useState(false);
  return (
    <span className={`tip-wrap ${open ? "is-open" : ""}`} onMouseLeave={() => setOpen(false)}>
      <button
        type="button"
        className="tip-btn"
        aria-label={`Spiegazione: ${title}`}
        aria-expanded={open}
        aria-controls={id}
        onClick={() => setOpen((v) => !v)}
        onKeyDown={(e) => {
          if (e.key === "Escape") setOpen(false);
        }}
        onBlur={() => setOpen(false)}
      >
        <Info size={13} aria-hidden="true" />
      </button>
      <span id={id} role="tooltip" className={`tip-bubble align-${align}`}>
        <strong>{title}:</strong> {text}
      </span>
    </span>
  );
}
