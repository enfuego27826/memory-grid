import { useState } from 'react';
import { Button, Card } from '../components/ui';
import { createRoom, joinValid } from '../net/api';
import { start } from '../net/WebSocketService';
import type { Difficulty } from '../types/settings';

export default function HomeScreen() {
  const [name, setName] = useState('');
  const [code, setCode] = useState('');
  const [difficulty, setDifficulty] = useState<Difficulty>('medium');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);
  const [howto, setHowto] = useState(false);

  const doCreate = async () => {
    if (!name.trim()) return setError('Enter a display name.');
    setBusy(true); setError('');
    try {
      const c = await createRoom({ difficulty });
      start(c, name.trim());
    } catch { setError('Could not create a lobby.'); setBusy(false); }
  };
  const doJoin = async () => {
    if (!name.trim() || !code.trim()) return setError('Enter a name and a room code.');
    setBusy(true); setError('');
    const ok = await joinValid(code.trim().toUpperCase());
    if (!ok) { setError('No such room.'); setBusy(false); return; }
    start(code.trim().toUpperCase(), name.trim());
  };

  return (
    <div className="min-h-full flex items-center justify-center p-6">
      <div className="w-full max-w-md space-y-5">
        <div className="text-center">
          <h1 className="text-4xl font-black tracking-tight text-emerald-400">Memory Grid</h1>
          <p className="text-slate-400 mt-1">Memorise the path. Walk it blind. Guide each other.</p>
        </div>

        <Card className="space-y-3">
          <input className="w-full bg-slate-900 rounded px-3 py-2 outline-none"
            placeholder="Display name" value={name} maxLength={32}
            onChange={(e) => setName(e.target.value)} />

          <div className="flex items-center gap-2">
            <span className="text-sm text-slate-400">Difficulty</span>
            <select className="bg-slate-900 rounded px-2 py-1 text-sm"
              value={difficulty} onChange={(e) => setDifficulty(e.target.value as Difficulty)}>
              <option value="easy">Easy</option>
              <option value="medium">Medium</option>
              <option value="hard">Hard</option>
            </select>
            <Button className="ml-auto" onClick={doCreate} disabled={busy}>Create Lobby</Button>
          </div>

          <div className="border-t border-slate-700 pt-3 flex gap-2">
            <input className="flex-1 bg-slate-900 rounded px-3 py-2 outline-none uppercase"
              placeholder="ROOM CODE" value={code} maxLength={6}
              onChange={(e) => setCode(e.target.value)} />
            <Button kind="ghost" onClick={doJoin} disabled={busy}>Join</Button>
          </div>

          {error && <div className="text-rose-400 text-sm">{error}</div>}
        </Card>

        <Card>
          <button className="w-full text-left font-semibold" onClick={() => setHowto(!howto)}>
            How to play {howto ? '▾' : '▸'}
          </button>
          {howto && (
            <ul className="text-sm text-slate-400 mt-2 list-disc pl-5 space-y-1">
              <li>A safe path lights up on the grid — memorise it before it vanishes.</li>
              <li>First to press <b>“I’m Ready — Walk It!”</b> becomes the walker (and earns a volunteer bonus).</li>
              <li>The walker clicks tiles one-by-one from start to finish; everyone else types hints in chat.</li>
              <li>A wrong step costs a life and resets to the start. Reach the end to score.</li>
            </ul>
          )}
        </Card>
      </div>
    </div>
  );
}
