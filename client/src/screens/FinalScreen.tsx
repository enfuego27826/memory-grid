import { useGameStore, amHost } from '../store/useGameStore';
import { send, leave } from '../net/WebSocketService';
import { Button, Card, ColourDot } from '../components/ui';

export default function FinalScreen() {
  const board = useGameStore((s) => s.leaderboard);
  const players = useGameStore((s) => s.players);
  const host = useGameStore(amHost);
  const colourOf = (id: number) => players.find((p) => p.id === id)?.colour ?? '#888';

  const mostHelpful = [...board].sort((a, b) => b.hintsFollowed - a.hintsFollowed)[0];
  const totalHints = board.reduce((n, e) => n + e.hintsFollowed, 0);

  return (
    <div className="min-h-full flex flex-col items-center p-6 max-w-2xl mx-auto">
      <h2 className="text-3xl font-black text-emerald-400 mb-1">Final Leaderboard</h2>
      {board[0] && <p className="text-slate-300 mb-5">🏆 Winner: <b>{board[0].name}</b></p>}

      <Card className="w-full">
        {board.map((e, i) => (
          <div key={e.playerId}
            className={`flex items-center gap-3 py-2 ${i === 0 ? 'text-emerald-300 font-bold' : ''}`}>
            <span className="w-6 text-slate-500">{i + 1}</span>
            <ColourDot colour={colourOf(e.playerId)} />
            <span>{e.name}</span>
            <span className="ml-auto tabular-nums">{e.total}</span>
            <span className="text-xs text-slate-500 w-28 text-right">{e.successfulWalks} walks · {e.hintsFollowed} hints</span>
          </div>
        ))}
      </Card>

      <div className="text-sm text-slate-400 mt-4 text-center">
        Total hints followed: {totalHints}
        {mostHelpful && mostHelpful.hintsFollowed > 0 && <> · Most helpful: <b>{mostHelpful.name}</b></>}
      </div>

      <div className="flex gap-3 mt-6">
        {host && <Button onClick={() => send({ type: 'start_game' })}>Play Again</Button>}
        <Button kind="ghost" onClick={leave}>Leave</Button>
      </div>
    </div>
  );
}
