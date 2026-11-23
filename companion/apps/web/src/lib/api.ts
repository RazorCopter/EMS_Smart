import type { TimeRange } from "./format";

export interface AuthUser {
  username: string;
  role: "viewer" | "admin";
  csrf_token: string;
}

export class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) {
    super(message);
    this.status = status;
  }
}

function readCookie(name: string): string | null {
  const hit = document.cookie
    .split("; ")
    .find((row) => row.startsWith(`${name}=`));
  return hit ? decodeURIComponent(hit.split("=")[1]) : null;
}

export async function apiFetch<T>(path: string, init: RequestInit = {}): Promise<T> {
  const headers = new Headers(init.headers);
  if (init.body && !headers.has("Content-Type")) {
    headers.set("Content-Type", "application/json");
  }
  const csrf = readCookie("afore_companion_csrf");
  if (csrf && init.method && init.method !== "GET") {
    headers.set("X-CSRF-Token", csrf);
  }
  const res = await fetch(path, {
    credentials: "include",
    cache: "no-store",
    ...init,
    headers,
  });
  if (!res.ok) {
    let msg = `HTTP ${res.status}`;
    try {
      const payload = await res.json();
      msg = payload.detail || msg;
    } catch {
      // keep status text
    }
    throw new ApiError(res.status, msg);
  }
  return res.json() as Promise<T>;
}

export function rangeQuery(range: TimeRange, customStart?: string, customEnd?: string): string {
  const params = new URLSearchParams({ range });
  if (range === "custom" && customStart && customEnd) {
    params.set("start", new Date(customStart).toISOString());
    params.set("end", new Date(customEnd).toISOString());
  }
  return params.toString();
}
