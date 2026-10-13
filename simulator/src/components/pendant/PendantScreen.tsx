"use client";

import { useMemo, useState } from "react";
import type { PageId, ScreenProfile } from "@/lib/screen-profiles";
import { PAGES } from "@/lib/screen-profiles";
import { cn } from "@/lib/utils";

type Axis = { name: string; wpos: number; wco: number };

const INITIAL_AXES: Axis[] = [
  { name: "X", wpos: 12.34, wco: 10 },
  { name: "Y", wpos: -45.678, wco: 20 },
  { name: "Z", wpos: 3.2, wco: 0 },
];

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
          {page !== "MPG" && page !== "OVERRIDE" && (
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
  roomy: boolean;
  onSelectAxis: (i: number) => void;
  onStep: (s: number) => void;
  onZero: (i: number) => void;
  onLaser: (d: number) => void;
}) {
  const steps = [0.01, 0.1, 1, 10];

  return (
    <div className="flex h-full flex-col gap-1">
      {roomy && (
        <div className="px-10 text-[9px] uppercase tracking-wider text-zinc-400">
          WPos&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;MPos
        </div>
      )}

      {axes.map((axis, i) => (
        <button
          key={axis.name}
          type="button"
          onClick={() => onSelectAxis(i)}
          className={cn(
            "flex items-center gap-2 rounded-sm border px-2 text-left transition-colors",
            roomy ? "py-2.5" : "py-2",
            i === activeAxis
              ? "border-amber-400 bg-[#1f1f1f]"
              : "border-[#222] bg-[#333]"
          )}
        >
          <span
            className={cn(
              "w-6 shrink-0 font-bold text-amber-300",
              roomy ? "text-xl" : "text-base"
            )}
          >
            {axis.name}
          </span>
          <div className="min-w-0 flex-1">
            <div
              className={cn(
                "tabular-nums text-white",
                roomy ? "text-xl" : "text-lg"
              )}
            >
              {axis.wpos.toFixed(3)}
            </div>
            {roomy && (
              <div className="text-[11px] tabular-nums text-sky-300/90">
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
            className="rounded-sm border border-[#111] bg-[#2f2f2f] px-2 py-2 text-[11px] font-semibold text-white"
          >
            {"<0>"}
          </span>
        </button>
      ))}

      {roomy && (
        <div className="mt-1 grid grid-cols-2 gap-1 sm:grid-cols-4">
          <TeleCell title="FEED mm/min" value={feedRate.toFixed(0)} />
          <TeleCell
            title="LASER S cmd"
            value={laserCmd.toFixed(0)}
            onClick={() => onLaser(10)}
          />
          <TeleCell
            title="LASER S act"
            value={laserAct.toFixed(0)}
            onClick={() => onLaser(-10)}
          />
          <TeleCell
            title="OVERRIDES"
            value={`F${feedOv} S${speedOv} R${rapidOv}`}
          />
        </div>
      )}

      <div className="mt-auto">
        <div className="mb-1 text-[9px] uppercase tracking-wider text-zinc-300">
          Step (mm)
        </div>
        <div className="grid grid-cols-4 gap-1">
          {steps.map((s) => (
            <button
              key={s}
              type="button"
              onClick={() => onStep(s)}
              className={cn(
                "rounded-sm border py-2 text-[11px] font-semibold",
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
      <div className="text-[12px] font-semibold tabular-nums text-white">
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
