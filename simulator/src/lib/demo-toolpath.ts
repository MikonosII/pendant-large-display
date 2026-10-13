/** Demo laser toolpath (absolute mm) — stands in for a parsed LightBurn/NC file. */

export type PathSeg = {
  x: number;
  y: number;
  /** true = cut (G1/G2 with S>0), false = rapid (G0) */
  cut: boolean;
};

/** Simple badge outline + inner mark, roughly 80×50 mm work area. */
export const DEMO_TOOLPATH: PathSeg[] = [
  // Approach
  { x: 5, y: 5, cut: false },
  // Outer rectangle
  { x: 5, y: 5, cut: true },
  { x: 75, y: 5, cut: true },
  { x: 75, y: 45, cut: true },
  { x: 5, y: 45, cut: true },
  { x: 5, y: 5, cut: true },
  // Move to diamond
  { x: 40, y: 10, cut: false },
  { x: 55, y: 25, cut: true },
  { x: 40, y: 40, cut: true },
  { x: 25, y: 25, cut: true },
  { x: 40, y: 10, cut: true },
  // Center cross
  { x: 30, y: 25, cut: false },
  { x: 50, y: 25, cut: true },
  { x: 40, y: 15, cut: false },
  { x: 40, y: 35, cut: true },
];

export function toolpathBounds(path: PathSeg[]) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const p of path) {
    minX = Math.min(minX, p.x);
    minY = Math.min(minY, p.y);
    maxX = Math.max(maxX, p.x);
    maxY = Math.max(maxY, p.y);
  }
  const pad = 4;
  return {
    minX: minX - pad,
    minY: minY - pad,
    maxX: maxX + pad,
    maxY: maxY + pad,
    width: maxX - minX + pad * 2,
    height: maxY - minY + pad * 2,
  };
}
