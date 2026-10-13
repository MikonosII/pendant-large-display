"use client";

import { useMemo, useState } from "react";
import type { PageId, ScreenProfile } from "@/lib/screen-profiles";
import { PAGES } from "@/lib/screen-profiles";
import {
  parseGcodeToolpath,
  toolpathBounds,
  type PathSeg,
} from "@/lib/gcode-path";
import { SAMPLE_JOB_NAME, SAMPLE_JOB_NC } from "@/lib/sample-job";
import { cn } from "@/lib/utils";

type Axis = { name: string; wpos: number; wco: number };

/** Start near the sample job so the laser cursor is in view. */
const INITIAL_AXES: Axis[] = [
  { name: "X", wpos: 40, wco: 0 },
  { name: "Y", wpos: 25, wco: 0 },
  { name: "Z", wpos: 0, wco: 0 },
];

const INITIAL_JOB = (() => {
  const { path, warnings } = parseGcodeToolpath(SAMPLE_JOB_NC);
  return { name: SAMPLE_JOB_NAME, path, warnings, source: SAMPLE_JOB_NC };
})();

type Props = {
  profile: ScreenProfile;
  scale: number;
};

export function PendantScreen({ profile, scale }: Props) {
  const [page, setPage] = useState<PageId>("MPG");
  const [mpgOn, setMpgOn] = useState(true);
  const [axes, setAxes] = useState(INITIAL_AXES);
  const [feedOverride, setFeedOverride] = useState(100);
  const [activeAxis, setActiveAxis] = useState(0);
  const [step, setStep] = useState(0.1);
  const [feedRate, setFeedRate] = useState(1200);
  const [laserCmd, setLaserCmd] = useState(650);
  const [speedOv, setSpeedOv] = useState(100);
  const [rapidOv] = useState(100);
  const [job, setJob] = useState(INITIAL_JOB);

  const contentH = useMemo(() => {
    const statusH = 32 + 2;
    return profile.logicalH - profile.headerH - statusH - profile.softBtnH;
  }, [profile]);

  const jog = (dir: 1 | -1) => {
    setAxes((prev) =>
      prev.map((a, i) =>
        i === activeAxis
          ? {
              ...a,
              wpos: Math.round((a.wpos + dir * step) * 1000) / 1000,
            }
          : a
      )
    );
    setFeedRate((f) => Math.max(0, f + dir * 10));
  };

  const zeroAxis = (i: number) => {
    setAxes((prev) =>
      prev.map((a, idx) =>
        idx === i ? { ...a, wco: a.wco + a.wpos, wpos: 0 } : a
      )
    );
  };

  const laserAct = Math.max(0, laserCmd - 2);

  return (
    <div
      className="relative overflow-hidden rounded-sm shadow-[0_20px_60px_rgba(0,0,0,0.45)] ring-1 ring-black/40"
      style={{
        width: profile.logicalW * scale,
        height: profile.logicalH * scale,
        background: "#2a2a2a",
        fontFamily: "var(--font-ibm-plex-mono), ui-monospace, monospace",
      }}
    >
      <div
        style={{
          width: profile.logicalW,
          height: profile.logicalH,
          zoom: scale,
        }}
      >
        <div
          className="flex border-b border-black/50 bg-[#1e1e1e]"
          style={{ height: profile.headerH }}
        >
          {PAGES.map((p) => (
            <button
              key={p}
              type="button"
              onClick={() => setPage(p)}
              className={cn(
                "flex-1 border-r border-black/40 text-[9px] font-semibold tracking-wide transition-colors last:border-r-0",
                page === p
                  ? "bg-[#3d5a80] text-white"
                  : "bg-[#2f2f2f] text-zinc-300 hover:bg-[#383838]"
              )}
            >
              {p === "POWER FEED" ? "FEED" : p}
            </button>
          ))}
        </div>

        <div
          className="relative overflow-hidden bg-[#404040] px-1.5 py-1"
          style={{ height: contentH }}
        >
          {page === "MPG" && (
            <MpgView
              axes={axes}
              activeAxis={activeAxis}
              step={step}
              feedRate={feedRate}
              laserCmd={laserCmd}
              laserAct={laserAct}
              feedOv={feedOverride}
              speedOv={speedOv}
              rapidOv={rapidOv}
              path={job.path}
              jobName={job.name}
              roomy={profile.logicalW >= 480}
              onSelectAxis={setActiveAxis}
              onStep={setStep}
              onZero={zeroAxis}
              onLaser={(d) =>
                setLaserCmd((s) => Math.min(1000, Math.max(0, s + d)))
              }
            />
          )}
          {page === "OVERRIDE" && (
            <OverrideView
              feed={feedOverride}
              speed={speedOv}
              onFeed={setFeedOverride}
              onSpeed={setSpeedOv}
            />
          )}
          {page === "GCODE" && (
            <GcodeView
              name={job.name}
              segCount={job.path.length}
              warnings={job.warnings}
              onLoad={(name, nc) => {
                const { path, warnings } = parseGcodeToolpath(nc);
                setJob({ name, path, warnings, source: nc });
              }}
              onReloadSample={() => setJob(INITIAL_JOB)}
            />
          )}
          {page !== "MPG" && page !== "OVERRIDE" && page !== "GCODE" && (
            <PlaceholderView title={page} profile={profile} />
          )}
        </div>

        <div className="flex items-stretch border-t border-black/40 bg-[#555]">
          <div className="flex min-w-0 flex-1 flex-col justify-center px-2 py-1">
            <div className="text-[13px] font-bold leading-none text-white">
              Idle
            </div>
            <div className="mt-0.5 truncate text-[9px] text-zinc-200">
              Laser · mm · {profile.shortName}
            </div>
          </div>
          <button
            type="button"
            onClick={() => setMpgOn((v) => !v)}
            className={cn(
              "w-[72px] border-l border-black/40 text-[12px] font-bold transition-colors",
              mpgOn ? "bg-emerald-600 text-white" : "bg-[#2f2f2f] text-zinc-200"
            )}
          >
            MPG
          </button>
        </div>

        <div
          className="flex gap-px bg-black"
          style={{ height: profile.softBtnH }}
        >
          <SoftBtn
            label={page === "MPG" ? "Hold" : "Back"}
            onClick={() => setPage("MPG")}
          />
          {page === "MPG" && (
            <>
              <SoftBtn label="− Jog" onClick={() => jog(-1)} />
              <SoftBtn label="+ Jog" onClick={() => jog(1)} />
            </>
          )}
          {page !== "MPG" && (
            <SoftBtn
              label="Reset"
              onClick={() => {
                setAxes(INITIAL_AXES);
                setFeedOverride(100);
                setSpeedOv(100);
                setLaserCmd(650);
              }}
            />
          )}
        </div>
      </div>
    </div>
  );
}

function SoftBtn({
  label,
  onClick,
}: {
  label: string;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      className="flex-1 bg-[#2a2a2a] text-[11px] font-semibold text-white active:bg-[#3d5a80]"
    >
      {label}
    </button>
  );
}

function ToolpathPreview({
  x,
  y,
  laserOn,
  path,
  jobName,
}: {
  x: number;
  y: number;
  laserOn: boolean;
  path: PathSeg[];
  jobName: string;
}) {
  const bounds = toolpathBounds(path);
  const vb = `${bounds.minX} ${bounds.minY} ${bounds.width} ${bounds.height}`;

  const cutD: string[] = [];
  const rapidD: string[] = [];
  let pen = "M";
  let lastCut: boolean | null = null;
  for (const seg of path) {
    if (lastCut === null || lastCut !== seg.cut) {
      pen = "M";
      lastCut = seg.cut;
    }
    const cmd = `${pen}${seg.x} ${seg.y}`;
    if (seg.cut) cutD.push(cmd);
    else rapidD.push(cmd);
    pen = "L";
  }

  return (
    <div className="relative min-h-0 flex-1 overflow-hidden rounded-sm border border-[#111] bg-[#1a1a1a]">
      <div className="absolute left-1.5 top-1 z-10 max-w-[90%] truncate text-[8px] uppercase tracking-wider text-zinc-500">
        Job · {jobName}
      </div>
      <svg
        viewBox={vb}
        className="h-full w-full"
        preserveAspectRatio="xMidYMid meet"
        style={{ transform: "scaleY(-1)" }}
      >
        <path
          d={rapidD.join(" ")}
          fill="none"
          stroke="#555"
          strokeWidth={0.6}
          strokeDasharray="2 1.5"
        />
        <path
          d={cutD.join(" ")}
          fill="none"
          stroke="#f59e0b"
          strokeWidth={0.9}
        />
        {/* Laser head */}
        <g>
          <circle
            cx={x}
            cy={y}
            r={2.2}
            fill={laserOn ? "#ef4444" : "#38bdf8"}
            stroke="#fff"
            strokeWidth={0.4}
          />
          <line
            x1={x - 4}
            y1={y}
            x2={x + 4}
            y2={y}
            stroke="#fff"
            strokeWidth={0.35}
          />
          <line
            x1={x}
            y1={y - 4}
            x2={x}
            y2={y + 4}
            stroke="#fff"
            strokeWidth={0.35}
          />
        </g>
      </svg>
    </div>
  );
}

function MpgView({
  axes,
  activeAxis,
  step,
  feedRate,
  laserCmd,
  laserAct,
  feedOv,
  speedOv,
  rapidOv,
  path,
  jobName,
  roomy,
  onSelectAxis,
  onStep,
  onZero,
  onLaser,
}: {
  axes: Axis[];
  activeAxis: number;
  step: number;
  feedRate: number;
  laserCmd: number;
  laserAct: number;
  feedOv: number;
  speedOv: number;
  rapidOv: number;
  path: PathSeg[];
  jobName: string;
  roomy: boolean;
  onSelectAxis: (i: number) => void;
  onStep: (s: number) => void;
  onZero: (i: number) => void;
  onLaser: (d: number) => void;
}) {
  const steps = [0.01, 0.1, 1, 10];
  const laserOn = laserCmd > 0;

  return (
    <div className="flex h-full flex-col gap-1">
      {roomy && (
        <ToolpathPreview
          x={axes[0].wpos}
          y={axes[1].wpos}
          laserOn={laserOn}
          path={path}
          jobName={jobName}
        />
      )}

      {roomy && (
        <div className="px-1 text-[9px] uppercase tracking-wider text-zinc-400">
          WPos / MPos
        </div>
      )}

      <div className={cn(roomy ? "grid grid-cols-3 gap-1" : "flex flex-col gap-1")}>
        {axes.map((axis, i) => (
          <button
            key={axis.name}
            type="button"
            onClick={() => onSelectAxis(i)}
            className={cn(
              "flex items-center gap-1.5 rounded-sm border px-1.5 text-left transition-colors",
              roomy ? "py-1.5" : "py-2",
              i === activeAxis
                ? "border-amber-400 bg-[#1f1f1f]"
                : "border-[#222] bg-[#333]"
            )}
          >
            <span
              className={cn(
                "shrink-0 font-bold text-amber-300",
                roomy ? "text-sm" : "text-base"
              )}
            >
              {axis.name}
            </span>
            <div className="min-w-0 flex-1">
              <div
                className={cn(
                  "tabular-nums text-white",
                  roomy ? "text-sm" : "text-lg"
                )}
              >
                {axis.wpos.toFixed(3)}
              </div>
              {roomy && (
                <div className="text-[9px] tabular-nums text-sky-300/90">
                  M {(axis.wpos + axis.wco).toFixed(3)}
                </div>
              )}
            </div>
            <span
              role="button"
              tabIndex={0}
              onClick={(e) => {
                e.stopPropagation();
                onZero(i);
              }}
              onKeyDown={(e) => {
                if (e.key === "Enter" || e.key === " ") {
                  e.stopPropagation();
                  onZero(i);
                }
              }}
              className="rounded-sm border border-[#111] bg-[#2f2f2f] px-1.5 py-1 text-[10px] font-semibold text-white"
            >
              {"<0>"}
            </span>
          </button>
        ))}
      </div>

      {roomy && (
        <div className="grid grid-cols-[1fr_2fr_1fr] gap-1">
          <TeleCell title="FEED mm/min" value={feedRate.toFixed(0)} />
          <LaserPair
            cmd={Math.min(100, Math.max(0, Math.round((laserCmd / 1000) * 100)))}
            act={Math.min(100, Math.max(0, Math.round((laserAct / 1000) * 100)))}
            onCmd={() => onLaser(10)}
            onAct={() => onLaser(-10)}
          />
          <TeleCell
            title="OVERRIDES"
            value={`F${feedOv} S${speedOv} R${rapidOv}`}
          />
        </div>
      )}

      <div>
        <div className="mb-0.5 text-[9px] uppercase tracking-wider text-zinc-300">
          Step (mm)
        </div>
        <div className="grid grid-cols-4 gap-1">
          {steps.map((s) => (
            <button
              key={s}
              type="button"
              onClick={() => onStep(s)}
              className={cn(
                "rounded-sm border py-1.5 text-[11px] font-semibold",
                step === s
                  ? "border-sky-400 bg-sky-700 text-white"
                  : "border-[#222] bg-[#2f2f2f] text-zinc-200"
              )}
            >
              {s}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}

function TeleCell({
  title,
  value,
  onClick,
}: {
  title: string;
  value: string;
  onClick?: () => void;
}) {
  const body = (
    <>
      <div className="text-[8px] uppercase tracking-wider text-zinc-400">
        {title}
      </div>
      <div className="text-right text-[12px] font-semibold tabular-nums text-white">
        {value}
      </div>
    </>
  );
  if (onClick) {
    return (
      <button
        type="button"
        onClick={onClick}
        className="rounded-sm border border-[#111] bg-[#2a2a2a] px-1.5 py-1 text-left"
      >
        {body}
      </button>
    );
  }
  return (
    <div className="rounded-sm border border-[#111] bg-[#2a2a2a] px-1.5 py-1 text-left">
      {body}
    </div>
  );
}

function LaserPair({
  cmd,
  act,
  onCmd,
  onAct,
}: {
  cmd: number;
  act: number;
  onCmd: () => void;
  onAct: () => void;
}) {
  const fmt = (n: number) => `${String(n).padStart(3, "\u00A0")}%`;
  return (
    <div className="rounded-sm border border-[#111] bg-[#2a2a2a] px-1.5 py-1">
      <div className="text-[8px] uppercase tracking-wider text-zinc-400">
        Laser
      </div>
      <div className="mt-0.5 grid grid-cols-2 gap-2">
        <button
          type="button"
          onClick={onCmd}
          className="flex items-baseline justify-between gap-1 text-left"
        >
          <span className="text-[8px] uppercase text-zinc-500">Cmd</span>
          <span className="whitespace-pre text-[12px] font-semibold tabular-nums text-white">
            {fmt(cmd)}
          </span>
        </button>
        <button
          type="button"
          onClick={onAct}
          className="flex items-baseline justify-between gap-1 text-left"
        >
          <span className="text-[8px] uppercase text-zinc-500">Act</span>
          <span className="whitespace-pre text-[12px] font-semibold tabular-nums text-white">
            {fmt(act)}
          </span>
        </button>
      </div>
    </div>
  );
}

function OverrideView({
  feed,
  speed,
  onFeed,
  onSpeed,
}: {
  feed: number;
  speed: number;
  onFeed: (n: number) => void;
  onSpeed: (n: number) => void;
}) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-4">
      <div className="text-center">
        <div className="text-[10px] uppercase tracking-widest text-zinc-300">
          Feed override
        </div>
        <div className="text-3xl font-bold tabular-nums text-white">{feed}%</div>
        <div className="mt-1 flex justify-center gap-2">
          {[-10, -1, 1, 10].map((d) => (
            <button
              key={`f${d}`}
              type="button"
              onClick={() => onFeed(Math.min(200, Math.max(10, feed + d)))}
              className="min-w-10 rounded-sm border border-[#222] bg-[#2f2f2f] px-2 py-1 text-sm font-semibold text-white"
            >
              {d > 0 ? `+${d}` : d}
            </button>
          ))}
        </div>
      </div>
      <div className="text-center">
        <div className="text-[10px] uppercase tracking-widest text-zinc-300">
          Laser / spindle override
        </div>
        <div className="text-3xl font-bold tabular-nums text-white">
          {speed}%
        </div>
        <div className="mt-1 flex justify-center gap-2">
          {[-10, -1, 1, 10].map((d) => (
            <button
              key={`s${d}`}
              type="button"
              onClick={() => onSpeed(Math.min(200, Math.max(10, speed + d)))}
              className="min-w-10 rounded-sm border border-[#222] bg-[#2f2f2f] px-2 py-1 text-sm font-semibold text-white"
            >
              {d > 0 ? `+${d}` : d}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}

function GcodeView({
  name,
  segCount,
  warnings,
  onLoad,
  onReloadSample,
}: {
  name: string;
  segCount: number;
  warnings: string[];
  onLoad: (name: string, nc: string) => void;
  onReloadSample: () => void;
}) {
  return (
    <div className="flex h-full flex-col gap-3 px-2 py-3 text-zinc-200">
      <div>
        <div className="text-[10px] uppercase tracking-widest text-zinc-400">
          Loaded job
        </div>
        <div className="mt-0.5 text-sm font-semibold text-white">{name}</div>
        <div className="text-[11px] tabular-nums text-zinc-300">
          {segCount} path segments
        </div>
      </div>
      {warnings.length > 0 && (
        <div className="text-[10px] leading-snug text-amber-300/90">
          {warnings.slice(0, 4).join(" · ")}
        </div>
      )}
      <label className="inline-flex cursor-pointer items-center justify-center rounded-sm border border-[#222] bg-[#2f2f2f] px-3 py-2 text-[11px] font-semibold text-white">
        Load .nc / .gcode
        <input
          type="file"
          accept=".nc,.gcode,.ngc,.txt"
          className="hidden"
          onChange={async (e) => {
            const file = e.target.files?.[0];
            if (!file) return;
            const text = await file.text();
            onLoad(file.name, text);
            e.target.value = "";
          }}
        />
      </label>
      <button
        type="button"
        onClick={onReloadSample}
        className="rounded-sm border border-[#222] bg-[#2a2a2a] px-3 py-2 text-[11px] font-semibold text-zinc-200"
      >
        Reload sample-badge.nc
      </button>
      <p className="text-[10px] leading-relaxed text-zinc-400">
        Preview parses G0/G1/G2/G3 with M3/M4/M5 and modal S. Cut moves (amber)
        vs rapids (dashed) update the MPG job canvas.
      </p>
    </div>
  );
}

function PlaceholderView({
  title,
  profile,
}: {
  title: string;
  profile: ScreenProfile;
}) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-2 px-4 text-center">
      <div className="text-sm font-bold text-white">{title}</div>
      <p className="text-[10px] leading-relaxed text-zinc-300">
        Layout chrome for {profile.name}. Content screens reuse upstream
        SmartPendant logic; this lab focuses on size and touch targets.
      </p>
    </div>
  );
}
