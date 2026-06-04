import type { Geom } from './geometry';
import { idxToRC, rc } from './geometry';
import type { FlashMode } from '../types/settings';

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

export function drawGrid(ctx: CanvasRenderingContext2D, g: Geom) {
  for (let r = 0; r < g.rows; r++) {
    for (let c = 0; c < g.cols; c++) {
      const { x, y } = rc(g, r, c);
      ctx.fillStyle = (r + c) % 2 ? '#141b30' : '#161d33';
      roundRect(ctx, x + 1, y + 1, g.cell - 2, g.cell - 2, Math.min(8, g.cell * 0.16));
      ctx.fill();
    }
  }
}

function fillTile(ctx: CanvasRenderingContext2D, g: Geom, row: number, col: number, fill: string, alpha = 1) {
  const { x, y } = rc(g, row, col);
  ctx.save();
  ctx.globalAlpha = alpha;
  ctx.fillStyle = fill;
  roundRect(ctx, x + 1, y + 1, g.cell - 2, g.cell - 2, Math.min(8, g.cell * 0.16));
  ctx.fill();
  ctx.restore();
}

// Anti-screenshot reveal: a moving "comet" traces the path start→finish on a
// loop, lighting only a short trailing window at any instant. The full path is
// therefore NEVER present in a single frame, so a screenshot captures only a few
// tiles; the human learns the route by watching it trace. (This is the real
// defence — a focus/blur blackout can't beat an OS capture, which grabs the
// frame before any JS event fires.) `mode` modulates the trace speed.
export function drawPath(
  ctx: CanvasRenderingContext2D, g: Geom, path: [number, number][], mode: FlashMode, t: number,
) {
  const n = path.length;
  if (n === 0) return;
  const msPerTile = mode === 'fading' ? 420 : mode === 'flashing' ? 240 : 320;
  const trail = Math.max(3, Math.round(n * 0.18));   // lit window (minority of path)
  const gap = trail + 2;                              // dark pause before re-tracing
  const head = (t / msPerTile) % (n + gap);          // floating head position
  for (let i = 0; i < n; i++) {
    const d = head - i;                               // distance behind the head
    if (d < 0 || d >= trail) continue;
    const alpha = Math.max(0.1, 0.95 * (1 - d / trail));
    const [row, col] = path[i];
    fillTile(ctx, g, row, col, '#38e0a6', alpha);
  }
}

export function drawWalk(
  ctx: CanvasRenderingContext2D, g: Geom, walked: number[], walkerPos: { row: number; col: number } | null,
  colour: string,
) {
  for (const idx of walked) {
    const { row, col } = idxToRC(idx, g.cols);
    fillTile(ctx, g, row, col, colour, 0.55);
  }
  if (walkerPos) {
    fillTile(ctx, g, walkerPos.row, walkerPos.col, colour, 0.95);
    const { x, y } = rc(g, walkerPos.row, walkerPos.col);
    ctx.save();
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 3;
    roundRect(ctx, x + 2, y + 2, g.cell - 4, g.cell - 4, Math.min(8, g.cell * 0.16));
    ctx.stroke();
    ctx.restore();
  }
}

export function drawMarkers(
  ctx: CanvasRenderingContext2D, g: Geom,
  start: { row: number; col: number } | null, finish: { row: number; col: number } | null,
) {
  const ring = (p: { row: number; col: number }, colour: string) => {
    const { x, y } = rc(g, p.row, p.col);
    ctx.save();
    ctx.strokeStyle = colour;
    ctx.lineWidth = 3;
    roundRect(ctx, x + 3, y + 3, g.cell - 6, g.cell - 6, Math.min(8, g.cell * 0.16));
    ctx.stroke();
    ctx.restore();
  };
  if (start) ring(start, '#7dd3fc');   // start: light blue
  if (finish) ring(finish, '#fbbf24'); // finish: amber
}
