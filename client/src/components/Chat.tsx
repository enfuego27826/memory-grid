import { useEffect, useRef, useState } from 'react';
import { useGameStore } from '../store/useGameStore';
import { send } from '../net/WebSocketService';

export default function Chat({ readOnly, rateLimitMs }: { readOnly: boolean; rateLimitMs: number }) {
  const chat = useGameStore((s) => s.chat);
  const lastSent = useGameStore((s) => s.lastChatSentAt);
  const markSent = useGameStore((s) => s.markChatSent);
  const [text, setText] = useState('');
  const [cooldown, setCooldown] = useState(0);
  const logRef = useRef<HTMLDivElement>(null);

  useEffect(() => { logRef.current?.scrollTo(0, logRef.current.scrollHeight); }, [chat]);

  // client-side cooldown FEEDBACK (server is authoritative and silently drops)
  useEffect(() => {
    const id = setInterval(() => {
      setCooldown(Math.max(0, rateLimitMs - (Date.now() - lastSent)));
    }, 100);
    return () => clearInterval(id);
  }, [lastSent, rateLimitMs]);

  const submit = () => {
    const m = text.trim();
    if (!m || cooldown > 0) return;
    send({ type: 'send_chat', message: m });
    markSent();
    setText('');
  };

  return (
    <div className="flex flex-col h-full min-h-0">
      <div ref={logRef} className="flex-1 overflow-y-auto space-y-1 text-sm pr-1">
        {chat.map((c) => (
          <div key={c.key} className={c.spectator ? 'opacity-50 italic' : ''}>
            <span className="font-semibold">{c.name}:</span> {c.text}
          </div>
        ))}
      </div>
      {readOnly ? (
        <div className="text-xs text-slate-500 mt-2 italic">You are walking — chat is read-only.</div>
      ) : (
        <div className="mt-2 flex gap-2">
          <input
            className="flex-1 bg-slate-900 rounded px-2 py-1 text-sm outline-none"
            value={text}
            placeholder={cooldown > 0 ? `wait ${(cooldown / 1000).toFixed(1)}s` : 'hint, e.g. "go right" or "B4"'}
            onChange={(e) => setText(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter') submit(); }}
          />
          <button className="px-3 py-1 rounded bg-emerald-500 text-slate-900 text-sm disabled:opacity-40"
            disabled={cooldown > 0} onClick={submit}>Send</button>
        </div>
      )}
    </div>
  );
}
