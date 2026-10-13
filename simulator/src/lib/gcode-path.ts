/** Minimal GRBL/LightBurn XY toolpath parser for job preview. */

export type PathSeg = {
  x: number;
  y: number;
  /** true = cut move (G1/G2/G3 with S>0), false = rapid/travel */
  cut: boolean;
};

export type ParseResult = {
  path: PathSeg[];
  warnings: string[];
};

const MAX_SEGS = 8192;

function words(line: string): { letter: string; value: number }[] {
  const out: { letter: string; value: number }[] = [];
  const re = /([A-Za-z])\s*([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(line)) !== null) {
    out.push({ letter: m[1].toUpperCase(), value: parseFloat(m[2]) });
  }
  return out;
}

function stripComments(line: string): string {
  const semi = line.indexOf(";");
  if (semi >= 0) line = line.slice(0, semi);
  const paren = line.indexOf("(");
  if (paren >= 0) line = line.slice(0, paren);
  return line.trim();
}

/** Linearize a CW/CCW arc (G2/G3) into chord segments in absolute mm. */
function arcPoints(
  x0: number,
  y0: number,
  x1: number,
  y1: number,
  i: number,
  j: number,
  cw: boolean,
  chordMm = 0.5
): { x: number; y: number }[] {
  const cx = x0 + i;
  const cy = y0 + j;
  const r0 = Math.hypot(x0 - cx, y0 - cy);
  if (!(r0 > 1e-6)) return [{ x: x1, y: y1 }];

  let a0 = Math.atan2(y0 - cy, x0 - cx);
  let a1 = Math.atan2(y1 - cy, x1 - cx);
  let da = a1 - a0;
  if (cw) {
    while (da >= 0) da -= Math.PI * 2;
  } else {
    while (da <= 0) da += Math.PI * 2;
  }
  if (Math.hypot(x1 - x0, y1 - y0) < 1e-6 && Math.hypot(i, j) > 1e-6) {
    da = cw ? -Math.PI * 2 : Math.PI * 2;
  }

  const arcLen = Math.abs(da) * r0;
  const n = Math.max(1, Math.min(180, Math.ceil(arcLen / chordMm)));
  const pts: { x: number; y: number }[] = [];
  for (let k = 1; k <= n; k++) {
    const a = a0 + (da * k) / n;
    pts.push({ x: cx + r0 * Math.cos(a), y: cy + r0 * Math.sin(a) });
  }
  pts[pts.length - 1] = { x: x1, y: y1 };
  return pts;
}

/**
 * Parse GRBL-ish laser G-code into XY segments.
 * Supports G0/G1/G2/G3, G90/G91, G20/G21, M3/M4/M5, modal S.
 */
export function parseGcodeToolpath(nc: string): ParseResult {
  const path: PathSeg[] = [];
  const warnings: string[] = [];

  let x = 0;
  let y = 0;
  let absolute = true;
  let unitScale = 1; // → mm
  let motion = 0; // 0=G0, 1=G1, 2=G2, 3=G3
  let s = 0;
  let spindleOn = false;
  let started = false;

  const push = (nx: number, ny: number, cut: boolean) => {
    if (path.length >= MAX_SEGS) return;
    if (!started) {
      path.push({ x: nx, y: ny, cut: false });
      started = true;
      x = nx;
      y = ny;
      return;
    }
    if (Math.hypot(nx - x, ny - y) < 1e-6) return;
    path.push({ x: nx, y: ny, cut });
    x = nx;
    y = ny;
  };

  const lines = nc.split(/\r?\n/);
  for (let li = 0; li < lines.length; li++) {
    const raw = stripComments(lines[li]);
    if (!raw) continue;
    const toks = words(raw);
    if (!toks.length) continue;

    let nx = x;
    let ny = y;
    let iOff = 0;
    let jOff = 0;
    let hasX = false;
    let hasY = false;
    let hasIJ = false;

    for (const t of toks) {
      switch (t.letter) {
        case "G": {
          const g = Math.round(t.value);
          if (g === 0 || g === 1 || g === 2 || g === 3) motion = g;
          else if (g === 90) absolute = true;
          else if (g === 91) absolute = false;
          else if (g === 20) unitScale = 25.4;
          else if (g === 21) unitScale = 1;
          break;
        }
        case "M": {
          const m = Math.round(t.value);
          if (m === 3 || m === 4) spindleOn = true;
          else if (m === 5) spindleOn = false;
          break;
        }
        case "S":
          s = t.value;
          break;
        case "X":
          nx = absolute ? t.value * unitScale : x + t.value * unitScale;
          hasX = true;
          break;
        case "Y":
          ny = absolute ? t.value * unitScale : y + t.value * unitScale;
          hasY = true;
          break;
        case "I":
          iOff = t.value * unitScale;
          hasIJ = true;
          break;
        case "J":
          jOff = t.value * unitScale;
          hasIJ = true;
          break;
        default:
          break;
      }
    }

    const moving = hasX || hasY || (hasIJ && (motion === 2 || motion === 3));
    if (!moving) continue;

    const cutting = motion !== 0 && spindleOn && s > 0;

    if (motion === 2 || motion === 3) {
      if (!hasIJ) {
        warnings.push(`line ${li + 1}: arc without I/J — treated as line`);
        push(nx, ny, cutting);
      } else {
        const pts = arcPoints(x, y, nx, ny, iOff, jOff, motion === 2);
        for (const p of pts) {
          if (path.length >= MAX_SEGS) {
            warnings.push("segment limit reached");
            break;
          }
          push(p.x, p.y, cutting);
        }
      }
    } else {
      push(nx, ny, cutting);
    }

    if (path.length >= MAX_SEGS) {
      warnings.push("segment limit reached");
      break;
    }
  }

  if (!path.length) warnings.push("no XY motion found");
  return { path, warnings };
}

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
  if (!Number.isFinite(minX)) {
    minX = 0;
    minY = 0;
    maxX = 10;
    maxY = 10;
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
