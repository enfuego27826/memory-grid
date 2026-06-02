import { useState } from 'react';
import { useGameStore, amWalker, playerName } from '../store/useGameStore';
import { send } from '../net/WebSocketService';
import GridCanvas from '../canvas/GridCanvas';
import Chat from '../components/Chat';
import { Leaderboard } from '../components/panels';
import { useStopwatch } from '../hooks/useCountdown';
import { useVisibilityGuard } from '../hooks/guards';

export default function WalkScreen() {
  useVisibilityGuard();
  const walker = useGameStore((s) => s.walkerId);
  const iWalk = useGameStore(amWalker);
  const myLives = useGameStore((s) => s.myLives);
  const players = useGameStore((s) => s.players);
  const settings = useGameStore((s) => s.settings);
  const startedAt = useGameStore((s) => s.walkStartedAt);
  const walkerName = useGameStore((s) => (s.walkerId != null ? playerName(s, s.walkerId) : '—'));
  const elapsed = useStopwatch(startedAt);
  const [showLb, setShowLb] = useState(true);

  const walkerLives = iWalk ? myLives : players.find((p) => p.id === walker)?.lives ?? 0;
  const onTile = (row: number, col: number) => { if (iWalk) send({ type: 'click_tile', row, col }); };

  return (
    <div className="min-h-full flex flex-col">
      <div className="flex items-center gap-4 px-4 py-2 bg-slate-900/70 text-sm">
        <span className="font-semibold text-emerald-400">{walkerName}</span>
        <span className="text-slate-400">walking</span>
        <span>❤ {walkerLives}</span>
        <span className="tabular-nums">⏱ {elapsed.toFixed(1)}s</span>
        <button className="ml-auto text-slate-400 md:hidden" onClick={() => setShowLb(!showLb)}>
          {showLb ? 'Hide' : 'Board'}
        </button>
      </div>

      <div className="flex-1 flex flex-col md:flex-row min-h-0">
        <div className="flex-1 p-3 min-h-[50vh]">
          <div className="w-full h-full max-w-3xl mx-auto aspect-square">
            <GridCanvas interactive={iWalk} onTile={onTile} />
          </div>
          {iWalk && <div className="text-center text-slate-400 text-sm mt-1">Click adjacent tiles to follow the path.</div>}
        </div>

        <div className="w-full md:w-80 border-t md:border-t-0 md:border-l border-slate-800 p-3 flex flex-col gap-3 min-h-0">
          {showLb && <div className="bg-slate-800/50 rounded-xl p-3"><Leaderboard /></div>}
          <div className="flex-1 min-h-[30vh] bg-slate-800/50 rounded-xl p-3">
            <Chat readOnly={iWalk && !settings?.walkerCanChat} rateLimitMs={settings?.chatRateLimit ?? 1500} />
          </div>
        </div>
      </div>
    </div>
  );
}
