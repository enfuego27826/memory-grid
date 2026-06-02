import { useGameStore, playerName } from '../store/useGameStore';

export default function EliminationScreen() {
  const eliminated = useGameStore((s) => s.eliminatedThisRound);
  const nameOf = useGameStore((s) => (id: number) => playerName(s, id));
  return (
    <div className="min-h-full flex flex-col items-center justify-center p-6">
      <h2 className="text-3xl font-black text-rose-400 mb-6">Elimination</h2>
      <div className="space-y-2">
        {eliminated.length === 0 && <div className="text-slate-400">No one eliminated.</div>}
        {eliminated.map((id) => (
          <div key={id} className="flex items-center gap-3 bg-slate-800 rounded-xl px-5 py-3 animate-pop">
            <span className="font-semibold">{nameOf(id)}</span>
            <span className="text-xs bg-rose-600 text-white rounded px-2 py-0.5">Eliminated</span>
          </div>
        ))}
      </div>
    </div>
  );
}
