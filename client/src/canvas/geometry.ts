export interface Geom { cell: number; offX: number; offY: number; rows: number; cols: number; }

export function computeGeom(w: number, h: number, rows: number, cols: number): Geom {
  const cell = Math.max(8, Math.floor(Math.min(w / cols, h / rows)));
  const gw = cell * cols, gh = cell * rows;
  return { cell, offX: Math.floor((w - gw) / 2), offY: Math.floor((h - gh) / 2), rows, cols };
}

export function hitTest(px: number, py: number, g: Geom): { row: number; col: number } | null {
  const col = Math.floor((px - g.offX) / g.cell);
  const row = Math.floor((py - g.offY) / g.cell);
  if (row < 0 || col < 0 || row >= g.rows || col >= g.cols) return null;
  return { row, col };
}

export const idxToRC = (idx: number, cols: number) => ({ row: Math.floor(idx / cols), col: idx % cols });
export const rc = (g: Geom, row: number, col: number) => ({ x: g.offX + col * g.cell, y: g.offY + row * g.cell });
