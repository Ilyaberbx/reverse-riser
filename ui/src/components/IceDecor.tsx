/**
    Decorative crystalline shards. They live only in the free margins of the frosted panel
    (never over the waveform, the orb or any control) and ignore the pointer.
*/

interface ShardProps {
  x: number;
  y: number;
  scale?: number;
  rotate?: number;
  variant?: 0 | 1 | 2;
  opacity?: number;
}

// Each shard is a handful of facets: [points, fill id]
const SHAPES: [string, string][][] = [
  [
    ["0,40 22,0 34,30", "iceA"],
    ["22,0 52,18 34,30", "iceB"],
    ["0,40 34,30 30,62", "iceC"],
    ["34,30 52,18 58,50 30,62", "iceA"],
  ],
  [
    ["0,22 18,0 26,26", "iceB"],
    ["18,0 40,10 26,26", "iceA"],
    ["0,22 26,26 14,44", "iceC"],
    ["26,26 40,10 44,34 14,44", "iceB"],
  ],
  [
    ["0,30 30,0 26,34", "iceA"],
    ["30,0 46,22 26,34", "iceC"],
    ["0,30 26,34 20,52", "iceB"],
  ],
];

function Shard({ x, y, scale = 1, rotate = 0, variant = 0, opacity = 0.9 }: ShardProps) {
  return (
    <g transform={`translate(${x} ${y}) rotate(${rotate}) scale(${scale})`} opacity={opacity}>
      {SHAPES[variant].map(([points, fill], i) => (
        <polygon key={i} points={points} fill={`url(#${fill})`} style={{ stroke: "var(--ice-edge)" }} strokeWidth={0.6 / scale} />
      ))}
    </g>
  );
}

/** A gradient stop coloured by the theme (--ice-* tokens in styles/themes). */
const Stop = ({ at, token }: { at: number; token: string }) => (
  <stop offset={at} style={{ stopColor: `var(--ice-${token})`, stopOpacity: `var(--ice-${token}-o)` }} />
);

function Defs() {
  return (
    <defs>
      <linearGradient id="iceA" x1="0" y1="0" x2="1" y2="1">
        <Stop at={0} token="a0" />
        <Stop at={0.55} token="a1" />
        <Stop at={1} token="a2" />
      </linearGradient>
      <linearGradient id="iceB" x1="1" y1="0" x2="0" y2="1">
        <Stop at={0} token="b0" />
        <Stop at={1} token="b1" />
      </linearGradient>
      <linearGradient id="iceC" x1="0" y1="1" x2="1" y2="0">
        <Stop at={0} token="c0" />
        <Stop at={1} token="c1" />
      </linearGradient>
      <filter id="frostNoise" x="0" y="0" width="100%" height="100%">
        <feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="4" />
        <feColorMatrix type="matrix" values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 0.05 0" />
      </filter>
    </defs>
  );
}

/** Shards in the dark margin around the panel (behind it). */
export function BackdropIce() {
  return (
    <svg className="backdrop-ice" viewBox="0 0 700 860" preserveAspectRatio="none" aria-hidden="true">
      <Defs />
      <Shard x={-14} y={780} scale={1.5} rotate={-12} variant={0} opacity={0.55} />
      <Shard x={652} y={788} scale={1.4} rotate={18} variant={1} opacity={0.5} />
      <Shard x={660} y={-10} scale={1.1} rotate={40} variant={2} opacity={0.4} />
      <Shard x={-10} y={-6} scale={0.9} rotate={-30} variant={1} opacity={0.35} />
    </svg>
  );
}

/** Shards resting on the panel's free corners, plus a faint frost grain. */
export function PanelIce() {
  return (
    <svg className="panel-ice" viewBox="0 0 676 836" aria-hidden="true">
      <Defs />
      <rect width="676" height="836" filter="url(#frostNoise)" />
      {/* lower left, outside the effect bar */}
      <Shard x={14} y={758} scale={1.2} rotate={-8} variant={0} />
      <Shard x={86} y={786} scale={0.7} rotate={22} variant={2} opacity={0.75} />
      {/* lower right */}
      <Shard x={604} y={752} scale={1.25} rotate={14} variant={1} />
      <Shard x={556} y={788} scale={0.6} rotate={-18} variant={0} opacity={0.75} />
      {/* left and right edges beside the orb */}
      <Shard x={34} y={380} scale={0.9} rotate={-20} variant={2} opacity={0.6} />
      <Shard x={608} y={396} scale={0.85} rotate={25} variant={0} opacity={0.6} />
    </svg>
  );
}
