// Anti-screenshot noise overlay (best-effort, spec §12).
//
// Honest scope: this is a DETERRENT, not a guarantee. We draw a per-frame,
// low-amplitude, full-frequency noise field that REGENERATES every frame. At
// 60fps the eye temporally integrates successive opposite-ish noise frames
// toward grey (flicker fusion) so the grid looks clean live; a screenshot
// freezes ONE frame and captures the full-amplitude noise, and many recorders
// alias the high-frequency temporal noise into visible shimmer. The real
// protection is structural (path never in the DOM, dropped on phase change,
// server-authoritative) — see the store + game core.

const TILE = 64;
let tile: HTMLCanvasElement | null = null;
let tctx: CanvasRenderingContext2D | null = null;
let img: ImageData | null = null;

// Amplitude knob: higher = more screenshot-visible but more perceptible live.
const AMPLITUDE = 0.05;

function ensureTile() {
  if (tile) return;
  tile = document.createElement('canvas');
  tile.width = TILE; tile.height = TILE;
  tctx = tile.getContext('2d');
  img = tctx!.createImageData(TILE, TILE);
}

export function drawNoise(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number) {
  ensureTile();
  const data = img!.data;
  for (let i = 0; i < data.length; i += 4) {
    const v = (Math.random() * 255) | 0;
    data[i] = data[i + 1] = data[i + 2] = v;
    data[i + 3] = 255;
  }
  tctx!.putImageData(img!, 0, 0);
  ctx.save();
  ctx.globalAlpha = AMPLITUDE;
  ctx.globalCompositeOperation = 'overlay';
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(tile!, x, y, w, h);
  ctx.restore();
}
