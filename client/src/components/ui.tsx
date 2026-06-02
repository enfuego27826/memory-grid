import { useState } from 'react';

export function Button({ children, onClick, disabled, kind = 'primary', className = '' }: {
  children: React.ReactNode; onClick?: () => void; disabled?: boolean;
  kind?: 'primary' | 'ghost' | 'danger'; className?: string;
}) {
  const base = 'px-4 py-2 rounded-lg font-semibold transition disabled:opacity-40 disabled:cursor-not-allowed';
  const kinds = {
    primary: 'bg-emerald-500 hover:bg-emerald-400 text-slate-900',
    ghost: 'bg-slate-700 hover:bg-slate-600 text-slate-100',
    danger: 'bg-rose-600 hover:bg-rose-500 text-white',
  };
  return (
    <button className={`${base} ${kinds[kind]} ${className}`} onClick={onClick} disabled={disabled}>
      {children}
    </button>
  );
}

export function Card({ children, className = '' }: { children: React.ReactNode; className?: string }) {
  return <div className={`bg-slate-800/60 rounded-2xl p-5 shadow-lg ${className}`}>{children}</div>;
}

export function ColourDot({ colour }: { colour: string }) {
  return <span className="inline-block w-3 h-3 rounded-full align-middle" style={{ background: colour }} />;
}

export function CopyButton({ text }: { text: string }) {
  const [done, setDone] = useState(false);
  return (
    <button
      className="ml-2 text-sm px-2 py-1 rounded bg-slate-700 hover:bg-slate-600"
      onClick={() => { navigator.clipboard?.writeText(text); setDone(true); setTimeout(() => setDone(false), 1200); }}
    >
      {done ? 'Copied!' : 'Copy'}
    </button>
  );
}
