import { useGameStore } from '../store/useGameStore';
import type { ServerMsg } from '../types/wire';

// Parse one inbound frame and route it into the store. Persists the session
// token so a page reload can resume the same player.
export function dispatch(raw: string) {
  let msg: ServerMsg;
  try {
    msg = JSON.parse(raw) as ServerMsg;
  } catch {
    return;
  }
  if (msg.type === 'joined') {
    try { sessionStorage.setItem('mg_token', msg.token); } catch { /* ignore */ }
  }
  useGameStore.getState().apply(msg);
}
