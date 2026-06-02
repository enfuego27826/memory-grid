import type { GameSettings } from '../types/settings';

// HTTP handshake helpers (same-origin: mg_server serves the client + the API).
export async function createRoom(
  settings?: Partial<GameSettings> & { difficulty: string },
  password?: string,
): Promise<string> {
  const body: Record<string, unknown> = {};
  if (settings) body.settings = settings;
  if (password) body.password = password;
  const r = await fetch('/create', { method: 'POST', body: JSON.stringify(body) });
  if (!r.ok) throw new Error('create failed');
  return (await r.json()).code as string;
}

export async function joinValid(code: string, password?: string): Promise<boolean> {
  const r = await fetch('/join', { method: 'POST', body: JSON.stringify({ code, password }) });
  return r.ok;
}
