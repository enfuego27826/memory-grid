import { useGameStore, myPlayer } from '../store/useGameStore';
import { send } from '../net/WebSocketService';
import GridCanvas from '../canvas/GridCanvas';
import { useCountdown } from '../hooks/useCountdown';
import { useDevtoolsGuard, useFocusBlackout } from '../hooks/guards';

export default function PatternScreen() {
  const players = useGameStore((s) => s.players);
  const me = useGameStore(myPlayer);
  const deadline = useGameStore((s) => s.memoriseDeadline);
  const hidden = useGameStore((s) => s.patternHidden);
  const remaining = useCountdown(deadline);
  useDevtoolsGuard(true);
  const blacked = useFocusBlackout(true);  // black out when the window isn't focused

  const stillDeciding = players.filter((p) => !p.isSpectator && !p.isEliminated && !p.isReady).length;
  const canVolunteer = me && !me.isSpectator && !me.isEliminated;

  return (
    <div className="min-h-full flex flex-col items-center p-4">
      <div className="text-center mb-2">
        <div className="text-6xl font-black tabular-nums text-emerald-400">{Math.ceil(remaining)}</div>
        <div className="text-slate-400 text-sm">Memorise the path · {stillDeciding} still deciding</div>
      </div>

      <div className="relative w-full max-w-2xl aspect-square no-pointer">
        <GridCanvas />
        {hidden && (
          <div className="absolute inset-0 flex items-center justify-center text-rose-400 font-semibold bg-slate-900/80">
            Pattern hidden (dev-tools detected)
          </div>
        )}
      </div>

      <button
        className="ready-button mt-5 px-8 py-4 rounded-2xl text-xl font-black bg-emerald-500 text-slate-900 hover:bg-emerald-400 disabled:opacity-40"
        disabled={!canVolunteer}
        onClick={() => send({ type: 'press_ready_walk' })}
      >
        I’m Ready — Walk It!
      </button>
      {!canVolunteer && <div className="text-slate-500 text-sm mt-2">Spectating this round.</div>}

      {/* Anti-screenshot: cover everything in opaque black when the window loses
          focus / the tab is hidden during the memorise phase. */}
      {blacked && (
        <div className="fixed inset-0 bg-black z-[100] flex items-center justify-center">
          <span className="text-slate-600 text-sm">Pattern hidden — focus the window</span>
        </div>
      )}
    </div>
  );
}
