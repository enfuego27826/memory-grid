import { useEffect, useState } from 'react';

// Display-only countdown to a deadline (epoch ms). The server is authoritative
// for the actual phase transition; this is purely cosmetic.
export function useCountdown(deadline: number | null): number {
  const [remaining, setRemaining] = useState(0);
  useEffect(() => {
    if (deadline == null) { setRemaining(0); return; }
    const tick = () => setRemaining(Math.max(0, (deadline - Date.now()) / 1000));
    tick();
    const id = setInterval(tick, 100);
    return () => clearInterval(id);
  }, [deadline]);
  return remaining;
}

// Count-up since a start time (epoch ms), in seconds.
export function useStopwatch(startedAt: number | null): number {
  const [elapsed, setElapsed] = useState(0);
  useEffect(() => {
    if (startedAt == null) { setElapsed(0); return; }
    const tick = () => setElapsed((Date.now() - startedAt) / 1000);
    tick();
    const id = setInterval(tick, 100);
    return () => clearInterval(id);
  }, [startedAt]);
  return elapsed;
}
