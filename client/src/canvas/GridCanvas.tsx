import { useEffect, useRef } from 'react';
import { computeGeom, hitTest, type Geom } from './geometry';
import { drawGrid, drawPath, drawWalk, drawMarkers } from './gridRender';
import { drawNoise } from './noiseOverlay';
import { useGameStore } from '../store/useGameStore';

interface Props {
  interactive?: boolean;                          // walk phase: clicks enabled
  onTile?: (row: number, col: number) => void;
}

// Canvas reads live store state inside the RAF loop (no React re-render churn).
export default function GridCanvas({ interactive, onTile }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const wrapRef = useRef<HTMLDivElement>(null);
  const geomRef = useRef<Geom | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current!;
    const wrap = wrapRef.current!;
    const ctx = canvas.getContext('2d')!;
    let raf = 0;

    const resize = () => {
      const dpr = window.devicePixelRatio || 1;
      const w = wrap.clientWidth, h = wrap.clientHeight;
      canvas.width = Math.floor(w * dpr);
      canvas.height = Math.floor(h * dpr);
      canvas.style.width = `${w}px`;
      canvas.style.height = `${h}px`;
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    };
    const ro = new ResizeObserver(resize);
    ro.observe(wrap);
    resize();

    const frame = (t: number) => {
      const s = useGameStore.getState();
      const cols = s.settings?.gridSize.cols ?? 6;
      const rows = s.settings?.gridSize.rows ?? 6;
      const w = wrap.clientWidth, h = wrap.clientHeight;
      const g = computeGeom(w, h, rows, cols);
      geomRef.current = g;

      ctx.clearRect(0, 0, w, h);
      drawGrid(ctx, g);

      const showPattern = s.phase === 'pattern' && s.patternPath && !s.patternHidden;
      if (showPattern) drawPath(ctx, g, s.patternPath!, s.flashMode, t);

      if (s.phase === 'walk') {
        const walker = s.players.find((p) => p.id === s.walkerId);
        drawWalk(ctx, g, s.walkedTiles, s.walkerPos, walker?.colour ?? '#60a5fa');
      }
      drawMarkers(ctx, g, s.startTile, s.finishTile);

      // Noise overlay during the memorise phase (anti-screenshot deterrent).
      if (showPattern) drawNoise(ctx, g.offX, g.offY, g.cell * cols, g.cell * rows);

      raf = requestAnimationFrame(frame);
    };
    raf = requestAnimationFrame(frame);
    return () => { cancelAnimationFrame(raf); ro.disconnect(); };
  }, []);

  const handlePointer = (e: React.PointerEvent) => {
    if (!interactive || !onTile || !geomRef.current) return;
    const canvas = canvasRef.current!;
    const r = canvas.getBoundingClientRect();
    const px = (e.clientX - r.left) * (canvas.width / r.width) / (window.devicePixelRatio || 1);
    const py = (e.clientY - r.top) * (canvas.height / r.height) / (window.devicePixelRatio || 1);
    const hit = hitTest(px, py, geomRef.current);
    if (hit) onTile(hit.row, hit.col);
  };

  return (
    <div ref={wrapRef} className="w-full h-full canvas-layer">
      <canvas
        ref={canvasRef}
        onPointerDown={handlePointer}
        className={interactive ? 'cursor-pointer' : 'no-pointer'}
      />
    </div>
  );
}
