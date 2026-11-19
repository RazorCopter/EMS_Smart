import { Component, type ErrorInfo, type ReactNode } from "react";
import { AlertTriangle, RotateCcw } from "lucide-react";

interface Props {
  styleName: string;
  onFallbackToClassic: () => void;
  onResetDefault?: () => void;
  children: ReactNode;
}

interface State {
  hasError: boolean;
  message: string;
}

export class ThemeErrorBoundary extends Component<Props, State> {
  state: State = { hasError: false, message: "" };

  static getDerivedStateFromError(error: Error): State {
    return {
      hasError: true,
      message: error?.message || "Errore imprevisto durante il rendering del tema Emerald Control",
    };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    console.error("[ThemeErrorBoundary] Emerald variant error:", error, info);
  }

  componentDidUpdate(prevProps: Props): void {
    if (prevProps.styleName !== this.props.styleName && this.state.hasError) {
      this.setState({ hasError: false, message: "" });
    }
  }

  render(): ReactNode {
    if (!this.state.hasError) {
      return this.props.children;
    }

    return (
      <div className="card" role="alert" style={{ borderColor: "var(--safety-amber)" }}>
        <div style={{ display: "flex", alignItems: "flex-start", gap: 12 }}>
          <AlertTriangle size={20} color="var(--safety-amber)" aria-hidden="true" style={{ flex: "none", marginTop: 2 }} />
          <div style={{ flex: 1 }}>
            <strong style={{ fontSize: 14 }}>Fallback automatico disponibile · Emerald Control</strong>
            <p className="card-sub" style={{ marginTop: 4 }}>
              Il componente grafico Emerald ha riscontrato un&apos;anomalia ({this.state.message}). I tuoi dati e il
              monitoraggio SHADOW restano integri; la tua preferenza salvata non è stata sovrascritta.
            </p>
            <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginTop: 10 }}>
              <button
                type="button"
                className="btn primary"
                onClick={() => {
                  this.setState({ hasError: false, message: "" });
                  this.props.onFallbackToClassic();
                }}
              >
                <RotateCcw size={14} aria-hidden="true" />
                Passa temporaneamente a Classic
              </button>
              {this.props.onResetDefault && (
                <button
                  type="button"
                  className="btn"
                  onClick={() => {
                    this.setState({ hasError: false, message: "" });
                    this.props.onResetDefault?.();
                  }}
                >
                  Ripristina Classic predefinito
                </button>
              )}
            </div>
          </div>
        </div>
      </div>
    );
  }
}
