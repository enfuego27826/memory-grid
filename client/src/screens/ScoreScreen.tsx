import { useGameStore, playerName } from '../store/useGameStore';
import { Card } from '../components/ui';
import type { ScoreLine } from '../types/wire';

function WalkerCard({ line, name }: { line: ScoreLine; name: string }) {
  const row = (label: string, v: number, cls = '') => (
    <div className="flex justify-between"><span className="text-slate-400">{label}</span>
      <span className={`tabular-nums ${cls}`}>{v >= 0 ? '+' : ''}{v}</span></div>
  );
  return (
    <Card className="animate-pop">
      <div className="font-bold text-emerald-400 mb-2">{name} · walker</div>
      <div className="space-y-0.5 text-sm">
        {row('Base', line.base)}
        {row('Speed bonus', line.speed)}
        {row('Volunteer bonus', line.volunteer)}
        {row('Wrong-step deductions', -line.deductions, 'text-rose-400')}
        <div className="flex justify-between border-t border-slate-700 mt-1 pt-1 font-bold">
          <span>Round</span><span className="tabular-nums">{line.roundScore}</span>
        </div>
      </div>
    </Card>
  );
}

function HelperCard({ line, name }: { line: ScoreLine; name: string }) {
  return (
    <Card className="animate-pop">
      <div className="font-semibold mb-2">{name}</div>
      <div className="space-y-0.5 text-sm">
        <div className="flex justify-between"><span className="text-slate-400">Hints followed</span><span>{line.hintsFollowed}</span></div>
        <div className="flex justify-between"><span className="text-slate-400">Correct hints ignored</span><span>{line.hintsIgnored}</span></div>
        <div className="flex justify-between border-t border-slate-700 mt-1 pt-1 font-bold">
          <span>Round</span><span className="tabular-nums">{line.roundScore}</span>
        </div>
      </div>
    </Card>
  );
}

export default function ScoreScreen() {
  const scores = useGameStore((s) => s.roundScores);
  const walkerId = useGameStore((s) => s.walkerId);
  const nameOf = useGameStore((s) => (id: number) => playerName(s, id));
  const round = useGameStore((s) => s.currentRound);
  const total = useGameStore((s) => s.totalRounds);

  return (
    <div className="min-h-full p-6 max-w-4xl mx-auto">
      <h2 className="text-2xl font-bold mb-4">Round {round} of {total} — Scores</h2>
      <div className="grid sm:grid-cols-2 gap-4">
        {scores.map((line) => (
          line.playerId === walkerId
            ? <WalkerCard key={line.playerId} line={line} name={nameOf(line.playerId)} />
            : <HelperCard key={line.playerId} line={line} name={nameOf(line.playerId)} />
        ))}
      </div>
      <div className="text-center text-slate-500 mt-6">Next round shortly…</div>
    </div>
  );
}
