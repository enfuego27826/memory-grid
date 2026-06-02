import { useGameStore } from '../store/useGameStore';
import { dispatch } from './dispatch';
import type { ClientMsg } from '../types/wire';

let ws: WebSocket | null = null;
let attempt = 0;
let intentional = false;
let queue: string[] = [];

function wsUrl(): string {
  const scheme = location.protocol === 'https:' ? 'wss' : 'ws';
  return `${scheme}://${location.host}/ws`;
}

function raw(text: string) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(text);
  else queue.push(text);
}

function open() {
  const store = useGameStore.getState();
  store.setStatus(attempt > 0 ? 'reconnecting' : 'connecting');
  intentional = false;
  ws = new WebSocket(wsUrl());

  ws.onopen = () => {
    attempt = 0;
    useGameStore.getState().setStatus('open');
    const s = useGameStore.getState();
    const token = s.token ?? sessionStorage.getItem('mg_token') ?? undefined;
    // (re)bind: first connect has no token; reconnect carries it.
    raw(JSON.stringify({ type: 'join_room', code: s.roomCode, name: s.myName, token }));
    const flush = queue; queue = [];
    flush.forEach(raw);
  };
  ws.onmessage = (e) => dispatch(typeof e.data === 'string' ? e.data : '');
  ws.onclose = () => {
    if (intentional) { useGameStore.getState().setStatus('closed'); return; }
    useGameStore.getState().setStatus('reconnecting');
    const delay = Math.min(1000 * 2 ** attempt, 15000) + Math.random() * 250;
    attempt += 1;
    setTimeout(open, delay);
  };
  ws.onerror = () => ws?.close();
}

// Start a fresh session (after create/join from the Home screen).
export function start(code: string, name: string) {
  sessionStorage.setItem('mg_code', code);
  sessionStorage.setItem('mg_name', name);
  sessionStorage.removeItem('mg_token');
  useGameStore.getState().setIdentity(name, code);
  attempt = 0;
  open();
}

// Resume after a page reload if we have a stored session.
export function resume(): boolean {
  const code = sessionStorage.getItem('mg_code');
  const name = sessionStorage.getItem('mg_name');
  const token = sessionStorage.getItem('mg_token');
  if (!code || !name) return false;
  useGameStore.setState({ token: token ?? null });
  useGameStore.getState().setIdentity(name, code);
  attempt = 0;
  open();
  return true;
}

export function send(msg: ClientMsg) {
  raw(JSON.stringify(msg));
}

export function leave() {
  intentional = true;
  ws?.close();
  ws = null;
  queue = [];
  ['mg_code', 'mg_name', 'mg_token'].forEach((k) => sessionStorage.removeItem(k));
  useGameStore.getState().reset();
}
