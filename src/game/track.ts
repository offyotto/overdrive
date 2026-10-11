import { add, cross, length, mix3, mod, normalize, scale, sub, type Vec3 } from '../engine/math.ts';

export interface TrackSample { position: Vec3; tangent: Vec3; right: Vec3; distance: number; }
export const TRACK_WIDTH = 15;
export const CONTROL_POINTS: Vec3[] = [
  [-75, 20, 94], [6, 23, 111], [78, 28, 87], [109, 36, 29],
  [97, 45, -35], [53, 48, -88], [-20, 42, -110], [-91, 32, -81],
  [-109, 24, -24], [-65, 23, 9], [-15, 27, -20], [16, 29, 14],
  [-19, 24, 49], [-74, 20, 52],
];
function catmull(p0: Vec3, p1: Vec3, p2: Vec3, p3: Vec3, t: number): Vec3 {
  const t2 = t * t, t3 = t2 * t;
  return [0, 1, 2].map(i => .5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3)) as Vec3;
}
export class Track {
  samples: TrackSample[] = [];
  length = 0;
  readonly width = TRACK_WIDTH;
  readonly boostPads: number[];
  readonly pickupZones: number[];
  constructor() {
    const points: Vec3[] = [];
    for (let i = 0; i <= 840; i++) {
      const p = i / 840 * CONTROL_POINTS.length, k = Math.floor(p), t = p - k;
      const at = (n: number) => CONTROL_POINTS[mod(n, CONTROL_POINTS.length)];
      points.push(catmull(at(k - 1), at(k), at(k + 1), at(k + 2), t));
    }
    for (let i = 0; i < points.length; i++) {
      if (i > 0) this.length += length(sub(points[i], points[i - 1]));
      const tangent = normalize(sub(points[(i + 1) % (points.length - 1)], points[mod(i - 1, points.length - 1)]));
      this.samples.push({ position: points[i], tangent, right: normalize(cross([0, 1, 0], tangent)), distance: this.length });
    }
    this.boostPads = [.14, .39, .62, .86].map(t => t * this.length);
    this.pickupZones = [.06, .3, .53, .78].map(t => t * this.length);
  }
  at(distance: number): TrackSample {
    const d = mod(distance, this.length);
    let lo = 0, hi = this.samples.length - 1;
    while (lo < hi - 1) {
      const mid = (lo + hi) >> 1;
      if (this.samples[mid].distance <= d) lo = mid; else hi = mid;
    }
    const a = this.samples[lo], b = this.samples[hi];
    const t = (d - a.distance) / (b.distance - a.distance || 1);
    return { position: mix3(a.position, b.position, t), tangent: normalize(mix3(a.tangent, b.tangent, t)), right: normalize(mix3(a.right, b.right, t)), distance: d };
  }
  position(distance: number, lateral = 0, lift = 0): Vec3 {
    const s = this.at(distance);
    return add(add(s.position, scale(s.right, lateral)), [0, lift, 0]);
  }
  curvature(distance: number): number {
    const a = this.at(distance).tangent, b = this.at(distance + 6).tangent;
    return Math.atan2(a[2] * b[0] - a[0] * b[2], a[0] * b[0] + a[2] * b[2]) / 6;
  }
  mapPath(width: number, height: number, padding = 12): string {
    return this.samples.filter((_, i) => i % 8 === 0).map((s, i) => `${i ? 'L' : 'M'}${(padding + (s.position[0] + 120) / 240 * (width - padding * 2)).toFixed(1)},${(padding + (s.position[2] + 120) / 240 * (height - padding * 2)).toFixed(1)}`).join(' ') + 'Z';
  }
}
