import { useGameStore, amHost } from '../store/useGameStore';
import { send, leave } from '../net/WebSocketService';
import { Button, Card, CopyButton } from '../components/ui';
import { PlayerList } from '../components/panels';
import Chat from '../components/Chat';
import { settingsRows, type Difficulty } from '../types/settings';

export default function LobbyScreen() {
  const code = useGameStore((s) => s.roomCode);
  const players = useGameStore((s) => s.players);
  const settings = useGameStore((s) => s.settings);
  const host = useGameStore(amHost);
  const canStart = players.filter((p) => !p.isSpectator).length >= 2;

  const setDifficulty = (d: Difficulty) => send({ type: 'configure_room', settings: { difficulty: d } });

  return (
    <div className="min-h-full p-6 grid md:grid-cols-3 gap-5 max-w-6xl mx-auto">
      <div className="md:col-span-2 space-y-5">
        <Card>
          <div className="flex items-center">
            <h2 className="text-xl font-bold">Lobby</h2>
            <span className="ml-3 font-mono text-2xl tracking-widest text-emerald-400">{code}</span>
            {code && <CopyButton text={code} />}
            <Button kind="ghost" className="ml-auto" onClick={leave}>Leave</Button>
          </div>
          <p className="text-slate-400 text-sm mt-1">Share the code. Game starts when the host launches it.</p>
        </Card>

        <Card>
          <h3 className="font-semibold mb-2">Players ({players.length})</h3>
          <PlayerList showHostControls />
        </Card>

        <Card>
          <h3 className="font-semibold mb-2">Pre-game chat</h3>
          <div className="h-40"><Chat readOnly={false} rateLimitMs={0} /></div>
        </Card>
      </div>

      <div className="space-y-5">
        <Card>
          <h3 className="font-semibold mb-2">Settings {host ? '(host)' : '(preview)'}</h3>
          {host && (
            <div className="flex gap-2 mb-3">
              {(['easy', 'medium', 'hard'] as Difficulty[]).map((d) => (
                <button key={d}
                  className={`px-2 py-1 rounded text-sm capitalize ${settings?.difficulty === d ? 'bg-emerald-500 text-slate-900' : 'bg-slate-700'}`}
                  onClick={() => setDifficulty(d)}>{d}</button>
              ))}
            </div>
          )}
          {settings && (
            <table className="w-full text-sm">
              <tbody>
                {settingsRows(settings).map(([k, v]) => (
                  <tr key={k}><td className="text-slate-400 py-0.5">{k}</td><td className="text-right">{v}</td></tr>
                ))}
              </tbody>
            </table>
          )}
        </Card>

        {host && (
          <Button className="w-full" disabled={!canStart} onClick={() => send({ type: 'start_game' })}>
            {canStart ? 'Start Game' : 'Need ≥2 players'}
          </Button>
        )}
        {!host && <div className="text-center text-slate-500 text-sm">Waiting for the host to start…</div>}
      </div>
    </div>
  );
}
