import { useGameStore, amHost } from '../store/useGameStore';
import { send } from '../net/WebSocketService';
import { ColourDot } from './ui';
import type { PlayerView } from '../types/wire';

export function PlayerList({ showHostControls }: { showHostControls?: boolean }) {
  const players = useGameStore((s) => s.players);
  const myId = useGameStore((s) => s.myId);
  const host = useGameStore(amHost);
  return (
    <div className="space-y-1">
      {players.map((p: PlayerView) => (
        <div key={p.id} className="flex items-center gap-2 text-sm py-1">
          <ColourDot colour={p.colour} />
          <span className={p.isSpectator ? 'text-slate-500' : ''}>
            {p.name}{p.id === myId ? ' (you)' : ''}
          </span>
          {p.isHost && <span className="text-amber-400 text-xs">host</span>}
          {p.isWalker && <span className="text-emerald-400 text-xs">walker</span>}
          {p.isEliminated && <span className="text-rose-400 text-xs">out</span>}
          <span className="ml-auto tabular-nums">{p.score}</span>
          {showHostControls && host && p.id !== myId && !p.isEliminated && (
            <span className="flex gap-1">
              <button className="text-xs text-slate-400 hover:text-amber-300"
                onClick={() => send({ type: 'transfer_host', target: p.id })}>★</button>
              <button className="text-xs text-slate-400 hover:text-rose-400"
                onClick={() => send({ type: 'kick_player', target: p.id })}>✕</button>
            </span>
          )}
        </div>
      ))}
    </div>
  );
}

export function Leaderboard() {
  const players = useGameStore((s) => s.players);
  const sorted = [...players].sort((a, b) => b.score - a.score);
  return (
    <div className="space-y-1 text-sm">
      <div className="font-semibold text-slate-300 mb-1">Leaderboard</div>
      {sorted.map((p, i) => (
        <div key={p.id} className="flex items-center gap-2">
          <span className="w-4 text-slate-500">{i + 1}</span>
          <ColourDot colour={p.colour} />
          <span className={p.isEliminated ? 'text-slate-500' : ''}>{p.name}</span>
          <span className="ml-auto tabular-nums">{p.score}</span>
        </div>
      ))}
    </div>
  );
}

export function NoticeFeed() {
  const notices = useGameStore((s) => s.notices);
  if (notices.length === 0) return null;
  return (
    <div className="fixed top-3 right-3 space-y-1 z-50 pointer-events-none">
      {notices.slice(-4).map((n) => (
        <div key={n.key} className="bg-slate-900/90 border border-slate-700 text-xs px-3 py-1.5 rounded-lg animate-pop">
          {n.text}
        </div>
      ))}
    </div>
  );
}

export function ConnectionBanner() {
  const status = useGameStore((s) => s.status);
  if (status === 'open' || status === 'idle') return null;
  const label = status === 'reconnecting' ? 'Reconnecting…'
    : status === 'connecting' ? 'Connecting…' : 'Disconnected';
  return (
    <div className="fixed bottom-0 inset-x-0 bg-amber-600/90 text-slate-900 text-center text-sm py-1 z-50">
      {label}
    </div>
  );
}
