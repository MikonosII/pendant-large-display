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
};

export function PendantScreen({ profile }: Props) {
  const [page, setPage] = useState<PageId>("MPG");
  const [mpgOn, setMpgOn] = useState(true);
  const [axes, setAxes] = useState(INITIAL_AXES);
  const [feedOverride, setFeedOverride] = useState(100);
  const [activeAxis, setActiveAxis] = useState(0);
  const [step, setStep] = useState(0.1);
  const [feedRate, setFeedRate] = useState(1200);
  const [laserCmd, setLaserCmd] = useState(650);
  const [speedOv, setSpeedOv] = useState(100);
  const [rapidOv, setRapidOv] = useState(100);
  const [powerFeed, setPowerFeed] = useState(300);
  const [job, setJob] = useState(INITIAL_JOB);
  const [startFlash, setStartFlash] = useState(false);
  const [framedTransport, setFramedTransport] = useState(false);
  const [probeFeed, setProbeFeed] = useState(100);
  const [probeTravel] = useState(20);
  const [probeStrict, setProbeStrict] = useState(true);
  const [probeContact, setProbeContact] = useState(false);
  const [probeResult, setProbeResult] = useState("Last [PRB:]: none");
  const [lfsOk] = useState(true);
  const [sdOk] = useState(false);

  const bumpOv = (
    kind: "feed" | "speed" | "rapid",
    cmd:
      | "reset"
      | "coarse+"
      | "coarse-"
      | "fine+"
      | "fine-"
      | "25"
      | "50"
  ) => {
    const apply = (v: number, d: number) =>
      Math.min(200, Math.max(10, v + d));
    if (kind === "rapid") {
      if (cmd === "reset") setRapidOv(100);
      else if (cmd === "50") setRapidOv(50);
      else if (cmd === "25") setRapidOv(25);
      return;
    }
    const set = kind === "feed" ? setFeedOverride : setSpeedOv;
    const cur = kind === "feed" ? feedOverride : speedOv;
    if (cmd === "reset") set(100);
    else if (cmd === "coarse+") set(apply(cur, 10));
    else if (cmd === "coarse-") set(apply(cur, -10));
    else if (cmd === "fine+") set(apply(cur, 1));
    else if (cmd === "fine-") set(apply(cur, -1));
  };

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
        // 1:1 logical pixels — CSS zoom/transform scale breaks pointer hit-testing
        width: profile.logicalW,
        height: profile.logicalH,
        background: "#2a2a2a",
        fontFamily: "var(--font-ibm-plex-mono), ui-monospace, monospace",
      }}
    >
      <div
        style={{
          width: profile.logicalW,
          height: profile.logicalH,
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
              onStart={() => {
                // Lab stand-in for realtime '~' cycle start
                setStartFlash(true);
                window.setTimeout(() => setStartFlash(false), 800);
              }}
              startFlash={startFlash}
            />
          )}
          {page === "OVERRIDE" && (
            <OverrideView
              feed={feedOverride}
              speed={speedOv}
              rapid={rapidOv}
              onOv={bumpOv}
            />
          )}
          {page === "POWER FEED" && (
            <FeedView
              axes={axes}
              activeAxis={activeAxis}
              powerFeed={powerFeed}
              onSelectAxis={setActiveAxis}
              onFeed={setPowerFeed}
              onJog={(dir) => {
                setAxes((prev) =>
                  prev.map((a, i) =>
                    i === activeAxis
                      ? {
                          ...a,
                          wpos:
                            Math.round((a.wpos + dir * 5) * 1000) / 1000,
                        }
                      : a
                  )
                );
              }}
            />
          )}
          {page === "GCODE" && (
            <GcodeView
              name={job.name}
              segCount={job.path.length}
              warnings={job.warnings}
              lfsOk={lfsOk}
              sdOk={sdOk}
              onLoad={(name, nc) => {
                const { path, warnings } = parseGcodeToolpath(nc);
                setJob({ name, path, warnings, source: nc });
              }}
              onReloadSample={() => setJob(INITIAL_JOB)}
            />
          )}
          {page === "PROBE" && (
            <ProbeView
              axes={axes}
              activeAxis={activeAxis}
              probeFeed={probeFeed}
              probeTravel={probeTravel}
              probeStrict={probeStrict}
              probeContact={probeContact}
              probeResult={probeResult}
              onSelectAxis={setActiveAxis}
              onFeed={setProbeFeed}
              onToggleStrict={() => setProbeStrict((v) => !v)}
              onProbe={(dir) => {
                setAxes((prev) =>
                  prev.map((a, i) =>
                    i === activeAxis
                      ? {
                          ...a,
                          wpos:
                            Math.round((a.wpos + dir * probeTravel) * 1000) /
                            1000,
                        }
                      : a
                  )
                );
                setProbeContact(true);
                const ax = axes[activeAxis];
                const t = Math.round((ax.wpos + dir * probeTravel) * 1000) / 1000;
                setProbeResult(
                  `Demo PRB  M ${axes
                    .map((a, i) =>
                      (i === activeAxis ? t : a.wpos) + a.wco
                    )
                    .map((v) => v.toFixed(3))
                    .join(", ")}  ok`
                );
              }}
              onZero={() => {
                zeroAxis(activeAxis);
                setProbeResult("Demo: WPos zeroed on axis");
              }}
            />
          )}
          {page === "SETTINGS" && (
            <SettingsView
              framed={framedTransport}
              onToggleFramed={() => setFramedTransport((v) => !v)}
              lfsOk={lfsOk}
              sdOk={sdOk}
            />
          )}
          {page !== "MPG" &&
            page !== "OVERRIDE" &&
            page !== "POWER FEED" &&
            page !== "GCODE" &&
            page !== "PROBE" &&
            page !== "SETTINGS" && (
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
          {page === "MPG" && (
            <>
              <SoftBtn label="Hold" onClick={() => setPage("MPG")} />
              <SoftBtn label="− Jog" onClick={() => jog(-1)} />
              <SoftBtn label="+ Jog" onClick={() => jog(1)} />
            </>
          )}
          {page === "OVERRIDE" && (
            <>
              <SoftBtn label="Back" onClick={() => setPage("MPG")} />
              <SoftBtn
                label="F −10"
                onClick={() => bumpOv("feed", "coarse-")}
              />
              <SoftBtn
                label="F +10"
                onClick={() => bumpOv("feed", "coarse+")}
              />
            </>
          )}
          {page === "POWER FEED" && (
            <>
              <SoftBtn label="Stop" onClick={() => setPage("MPG")} />
              <SoftBtn
                label="− Feed"
                onClick={() => {
                  setAxes((prev) =>
                    prev.map((a, i) =>
                      i === activeAxis
                        ? {
                            ...a,
                            wpos: Math.round((a.wpos - 5) * 1000) / 1000,
                          }
                        : a
                    )
                  );
                }}
              />
              <SoftBtn
                label="+ Feed"
                onClick={() => {
                  setAxes((prev) =>
                    prev.map((a, i) =>
                      i === activeAxis
                        ? {
                            ...a,
                            wpos: Math.round((a.wpos + 5) * 1000) / 1000,
                          }
                        : a
                    )
                  );
                }}
              />
            </>
          )}
          {page === "GCODE" && (
            <>
              <SoftBtn label="Back" onClick={() => setPage("MPG")} />
              <SoftBtn label="Refresh" onClick={() => undefined} />
              <SoftBtn
                label="Sample"
                onClick={() => setJob(INITIAL_JOB)}
              />
            </>
          )}
          {page === "PROBE" && (
            <>
              <SoftBtn label="Back" onClick={() => setPage("MPG")} />
              <SoftBtn
                label="Probe −"
                onClick={() => {
                  setAxes((prev) =>
                    prev.map((a, i) =>
                      i === activeAxis
                        ? {
                            ...a,
                            wpos:
                              Math.round((a.wpos - probeTravel) * 1000) / 1000,
                          }
                        : a
                    )
                  );
                  setProbeContact(true);
                  setProbeResult("Demo PRB − travel ok");
                }}
              />
              <SoftBtn
                label="Probe +"
                onClick={() => {
                  setAxes((prev) =>
                    prev.map((a, i) =>
                      i === activeAxis
                        ? {
                            ...a,
                            wpos:
                              Math.round((a.wpos + probeTravel) * 1000) / 1000,
                          }
                        : a
                    )
                  );
                  setProbeContact(true);
                  setProbeResult("Demo PRB + travel ok");
                }}
              />
            </>
          )}
          {page !== "MPG" &&
            page !== "OVERRIDE" &&
            page !== "POWER FEED" &&
            page !== "GCODE" &&
            page !== "PROBE" && (
              <>
                <SoftBtn label="Back" onClick={() => setPage("MPG")} />
                <SoftBtn
                  label="Reset"
                  onClick={() => {
                    setAxes(INITIAL_AXES);
                    setFeedOverride(100);
                    setSpeedOv(100);
                    setRapidOv(100);
                    setLaserCmd(650);
                    setProbeContact(false);
                  }}
                />
              </>
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
  onStart,
  startFlash,
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
  onStart: () => void;
  startFlash: boolean;
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

      <button
        type="button"
        onClick={onStart}
        className={cn(
          "mt-0.5 w-full rounded-sm border border-emerald-800 py-2.5 text-sm font-bold tracking-wide text-white transition-colors",
          startFlash ? "bg-emerald-400 text-emerald-950" : "bg-emerald-600"
        )}
      >
        START
      </button>
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

function OvRow({
  title,
  value,
  kind,
  onOv,
}: {
  title: string;
  value: number;
  kind: "feed" | "speed";
  onOv: (
    kind: "feed" | "speed" | "rapid",
    cmd:
      | "reset"
      | "coarse+"
      | "coarse-"
      | "fine+"
      | "fine-"
      | "25"
      | "50"
  ) => void;
}) {
  const btns: {
    label: string;
    cmd: "coarse-" | "fine-" | "reset" | "fine+" | "coarse+";
  }[] = [
    { label: "−10", cmd: "coarse-" },
    { label: "−1", cmd: "fine-" },
    { label: "100", cmd: "reset" },
    { label: "+1", cmd: "fine+" },
    { label: "+10", cmd: "coarse+" },
  ];
  return (
    <div className="w-full">
      <div className="text-[10px] uppercase tracking-widest text-zinc-400">
        {title}
      </div>
      <div className="text-3xl font-bold tabular-nums text-white">{value}%</div>
      <div className="mt-1 grid grid-cols-5 gap-1">
        {btns.map((b) => (
          <button
            key={b.label}
            type="button"
            onClick={() => onOv(kind, b.cmd)}
            className={cn(
              "rounded-sm border border-[#222] py-1.5 text-[11px] font-semibold text-white",
              b.cmd === "reset" ? "bg-[#3d5a80]" : "bg-[#2f2f2f]"
            )}
          >
            {b.label}
          </button>
        ))}
      </div>
    </div>
  );
}

function OverrideView({
  feed,
  speed,
  rapid,
  onOv,
}: {
  feed: number;
  speed: number;
  rapid: number;
  onOv: (
    kind: "feed" | "speed" | "rapid",
    cmd:
      | "reset"
      | "coarse+"
      | "coarse-"
      | "fine+"
      | "fine-"
      | "25"
      | "50"
  ) => void;
}) {
  return (
    <div className="flex h-full flex-col gap-4 overflow-y-auto px-1 py-2">
      <OvRow title="Feed override" value={feed} kind="feed" onOv={onOv} />
      <OvRow
        title="Laser / spindle"
        value={speed}
        kind="speed"
        onOv={onOv}
      />
      <div>
        <div className="text-[10px] uppercase tracking-widest text-zinc-400">
          Rapid
        </div>
        <div className="text-2xl font-bold tabular-nums text-white">
          {rapid}%
        </div>
        <div className="mt-1 grid grid-cols-3 gap-1">
          {(
            [
              ["25%", "25"],
              ["50%", "50"],
              ["100%", "reset"],
            ] as const
          ).map(([label, cmd]) => (
            <button
              key={label}
              type="button"
              onClick={() => onOv("rapid", cmd)}
              className={cn(
                "rounded-sm border border-[#222] py-1.5 text-[11px] font-semibold text-white",
                cmd === "reset" ? "bg-[#3d5a80]" : "bg-[#2f2f2f]"
              )}
            >
              {label}
            </button>
          ))}
        </div>
        <p className="mt-2 text-[9px] leading-snug text-zinc-500">
          Sends grblHAL realtime bytes (0x90–0x9D). Offline lab adjusts locally.
        </p>
      </div>
    </div>
  );
}

function FeedView({
  axes,
  activeAxis,
  powerFeed,
  onSelectAxis,
  onFeed,
  onJog,
}: {
  axes: Axis[];
  activeAxis: number;
  powerFeed: number;
  onSelectAxis: (i: number) => void;
  onFeed: (n: number) => void;
  onJog: (dir: 1 | -1) => void;
}) {
  return (
    <div className="flex h-full flex-col gap-3 px-1 py-2">
      <div className="text-[10px] uppercase tracking-widest text-zinc-400">
        Power feed
      </div>
      <div>
        <div className="mb-1 text-[9px] uppercase text-zinc-500">Axis</div>
        <div className="grid grid-cols-3 gap-1">
          {axes.map((a, i) => (
            <button
              key={a.name}
              type="button"
              onClick={() => onSelectAxis(i)}
              className={cn(
                "rounded-sm border py-3 text-sm font-bold",
                i === activeAxis
                  ? "border-amber-400 bg-[#3d5a80] text-white"
                  : "border-[#222] bg-[#2f2f2f] text-zinc-200"
              )}
            >
              {a.name}
              <div className="text-[10px] font-normal tabular-nums text-zinc-300">
                {a.wpos.toFixed(3)}
              </div>
            </button>
          ))}
        </div>
      </div>
      <div>
        <div className="text-[9px] uppercase text-zinc-500">Feed mm/min</div>
        <div className="text-3xl font-bold tabular-nums text-white">
          {powerFeed}
        </div>
        <div className="mt-1 grid grid-cols-4 gap-1">
          {[-100, -10, 10, 100].map((d) => (
            <button
              key={d}
              type="button"
              onClick={() =>
                onFeed(Math.min(10000, Math.max(10, powerFeed + d)))
              }
              className="rounded-sm border border-[#222] bg-[#2f2f2f] py-2 text-[11px] font-semibold text-white"
            >
              {d > 0 ? `+${d}` : d}
            </button>
          ))}
        </div>
      </div>
      <div className="grid grid-cols-2 gap-1">
        <button
          type="button"
          onClick={() => onJog(-1)}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-3 text-sm font-semibold text-white"
        >
          − Feed 5 mm
        </button>
        <button
          type="button"
          onClick={() => onJog(1)}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-3 text-sm font-semibold text-white"
        >
          + Feed 5 mm
        </button>
      </div>
    </div>
  );
}

function GcodeView({
  name,
  segCount,
  warnings,
  lfsOk,
  sdOk,
  onLoad,
  onReloadSample,
}: {
  name: string;
  segCount: number;
  warnings: string[];
  lfsOk: boolean;
  sdOk: boolean;
  onLoad: (name: string, nc: string) => void;
  onReloadSample: () => void;
}) {
  return (
    <div className="flex h-full flex-col gap-3 px-2 py-3 text-zinc-200">
      <div>
        <div className="text-[10px] uppercase tracking-widest text-zinc-400">
          G-code / jobs
        </div>
        <div className="mt-0.5 text-sm font-semibold text-white">{name}</div>
        <div className="text-[11px] tabular-nums text-zinc-300">
          {segCount} path segments
        </div>
        <div className="mt-1 text-[10px] text-zinc-400">
          LFS {lfsOk ? "ok" : "—"} · SD {sdOk ? "ok" : "—"} (lab: browser file)
        </div>
      </div>
      {warnings.length > 0 && (
        <div className="text-[10px] leading-snug text-amber-300/90">
          {warnings.slice(0, 4).join(" · ")}
        </div>
      )}
      <div className="rounded-sm border border-[#222] bg-[#2a2a2a] px-3 py-2 text-[11px]">
        <div className="font-semibold text-white">ROM · sample-badge.nc</div>
        <div className="text-[10px] text-zinc-400">embedded · always listed on device</div>
      </div>
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
        On device: LittleFS `/jobs` + SD `/jobs` (TF DIP mode). Lab uses a
        browser file picker; preview updates the MPG canvas.
      </p>
    </div>
  );
}

function ProbeView({
  axes,
  activeAxis,
  probeFeed,
  probeTravel,
  probeStrict,
  probeContact,
  probeResult,
  onSelectAxis,
  onFeed,
  onToggleStrict,
  onProbe,
  onZero,
}: {
  axes: Axis[];
  activeAxis: number;
  probeFeed: number;
  probeTravel: number;
  probeStrict: boolean;
  probeContact: boolean;
  probeResult: string;
  onSelectAxis: (i: number) => void;
  onFeed: (v: number) => void;
  onToggleStrict: () => void;
  onProbe: (dir: 1 | -1) => void;
  onZero: () => void;
}) {
  return (
    <div className="flex h-full flex-col gap-2 px-2 py-2 text-zinc-200">
      <div className="text-[10px] uppercase tracking-widest text-zinc-400">
        Probe
      </div>
      <div className="text-[9px] uppercase text-zinc-500">Axis</div>
      <div className="grid grid-cols-3 gap-1">
        {axes.map((a, i) => (
          <button
            key={a.name}
            type="button"
            onClick={() => onSelectAxis(i)}
            className={cn(
              "rounded-sm border border-[#222] py-3 text-sm font-bold text-white",
              i === activeAxis ? "bg-[#3d5a80]" : "bg-[#2f2f2f]"
            )}
          >
            {a.name}
          </button>
        ))}
      </div>
      <div className="text-[12px] tabular-nums text-white">
        F {probeFeed} · travel {probeTravel} mm ·{" "}
        {probeStrict ? "G38.2" : "G38.3"}
      </div>
      <div className="grid grid-cols-4 gap-1">
        {[-50, -10, 10, 50].map((d) => (
          <button
            key={d}
            type="button"
            onClick={() =>
              onFeed(Math.min(2000, Math.max(10, probeFeed + d)))
            }
            className="rounded-sm border border-[#222] bg-[#2f2f2f] py-2 text-[11px] font-semibold text-white"
          >
            {d > 0 ? `+${d}` : d}
          </button>
        ))}
      </div>
      <div className="grid grid-cols-2 gap-1">
        <button
          type="button"
          onClick={onToggleStrict}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-2 text-[11px] font-semibold text-white"
        >
          G38.2 / G38.3
        </button>
        <button
          type="button"
          onClick={onZero}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-2 text-[11px] font-semibold text-white"
        >
          Zero WPos
        </button>
      </div>
      <div
        className={cn(
          "text-[13px] font-semibold",
          probeContact ? "text-amber-400" : "text-zinc-400"
        )}
      >
        Contact: {probeContact ? "TRIGGERED" : "open"}
      </div>
      <div className="text-[11px] leading-snug text-zinc-300">{probeResult}</div>
      <div className="grid grid-cols-2 gap-1">
        <button
          type="button"
          onClick={() => onProbe(-1)}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-3 text-sm font-semibold text-white"
        >
          Probe −
        </button>
        <button
          type="button"
          onClick={() => onProbe(1)}
          className="rounded-sm border border-[#222] bg-[#2f2f2f] py-3 text-sm font-semibold text-white"
        >
          Probe +
        </button>
      </div>
      <p className="text-[10px] leading-relaxed text-zinc-400">
        Soft Probe −/+ send G38 toward WPos ± travel. Requires Idle + MPG.
        [PRB:] is always machine coordinates.
      </p>
    </div>
  );
}

function SettingsView({
  framed,
  onToggleFramed,
  lfsOk,
  sdOk,
}: {
  framed: boolean;
  onToggleFramed: () => void;
  lfsOk: boolean;
  sdOk: boolean;
}) {
  return (
    <div className="flex h-full flex-col gap-3 px-2 py-3 text-zinc-200">
      <div className="text-[10px] uppercase tracking-widest text-zinc-400">
        Settings
      </div>
      <div>
        <div className="text-[9px] uppercase text-zinc-500">UART transport</div>
        <button
          type="button"
          onClick={onToggleFramed}
          className={cn(
            "mt-1 w-full rounded-sm border border-[#222] py-3 text-sm font-bold text-white",
            framed ? "bg-[#3d5a80]" : "bg-[#2f2f2f]"
          )}
        >
          {framed ? "Framed" : "Plain"}
        </button>
        <p className="mt-2 text-[10px] leading-relaxed text-zinc-400">
          Plain matches stock grblHAL ASCII. Framed is Plugin_mpg_transport
          (SOF/CRC/ACK) — both ends must match. Lab toggle is visual only;
          firmware switches the real UART path.
        </p>
      </div>
      <div>
        <div className="text-[9px] uppercase text-zinc-500">Handwheel / I2C</div>
        <p className="mt-1 text-[10px] leading-relaxed text-zinc-400">
          GPIO A/B default −1 (TF card claims IO4–6). Prefer PCF8574 on
          SDA15/SCL16 (`I2C_MPG_ENABLE=1`, addr 0x20): P0/P1 wheel, P2–P4
          axis, P5 step cycle. Soft −/+ Jog stands in for detents in the lab.
        </p>
      </div>
      <div>
        <div className="text-[9px] uppercase text-zinc-500">Job store</div>
        <p className="mt-1 text-[10px] leading-relaxed text-zinc-400">
          LittleFS {lfsOk ? "ok" : "—"} · SD/TF {sdOk ? "mounted" : "—"} (lab
          mocks LFS). Device lists embedded sample + `/jobs` on LFS and SD.
        </p>
      </div>
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
