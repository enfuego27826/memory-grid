import { useEffect, useRef, useState } from 'react';
import { useGameStore, amWalker } from '../store/useGameStore';
import { send } from '../net/WebSocketService';

// Walker tab-switch detection (spec §7/§12): only fires while I am the walker
// during the walk. The server applies the freeze/penalty; we just signal.
export function useVisibilityGuard() {
  useEffect(() => {
    const onVis = () => {
      const s = useGameStore.getState();
      if (document.hidden && s.phase === 'walk' && amWalker(s)) send({ type: 'tab_switched' });
    };
    document.addEventListener('visibilitychange', onVis);
    return () => document.removeEventListener('visibilitychange', onVis);
  }, []);
}

// Anti-screenshot: while `active` (the memorise phase), report `true` whenever the
// window loses focus or the tab is hidden, so the UI can black the screen out
// completely. Screenshot/snipping tools that steal focus therefore capture black.
export function useFocusBlackout(active: boolean): boolean {
  const [blacked, setBlacked] = useState(false);
  useEffect(() => {
    if (!active) { setBlacked(false); return; }
    const onBlur = () => setBlacked(true);
    const onFocus = () => setBlacked(false);
    const onVis = () => setBlacked(document.hidden);
    window.addEventListener('blur', onBlur);
    window.addEventListener('focus', onFocus);
    document.addEventListener('visibilitychange', onVis);
    // initialise to the current state
    setBlacked(document.hidden || !document.hasFocus());
    return () => {
      window.removeEventListener('blur', onBlur);
      window.removeEventListener('focus', onFocus);
      document.removeEventListener('visibilitychange', onVis);
    };
  }, [active]);
  return blacked;
}

// Dev-tools heuristic during the memorise phase: a viewport shrink >100px is the
// classic signature of dev-tools opening — hide the pattern immediately.
export function useDevtoolsGuard(active: boolean) {
  const base = useRef<{ w: number; h: number } | null>(null);
  useEffect(() => {
    if (!active) { base.current = null; return; }
    base.current = { w: window.innerWidth, h: window.innerHeight };
    const onResize = () => {
      const b = base.current;
      if (!b) return;
      if (b.w - window.innerWidth > 100 || b.h - window.innerHeight > 100) {
        useGameStore.getState().hidePattern();
      }
    };
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, [active]);
}
