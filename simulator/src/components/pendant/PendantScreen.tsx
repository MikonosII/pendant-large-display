"use client";

import { useMemo, useState } from "react";
import type { PageId, ScreenProfile } from "@/lib/screen-profiles";
import { PAGES } from "@/lib/screen-profiles";
import { cn } from "@/lib/utils";

type Axis = { name: string; value: number };

const INITIAL_AXES: Axis[] = [
  { name: "X", value: 12.34 },
  { name: "Y", value: -45.678 },
  { name: "Z", value: 3.2 },
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

  const contentH = useMemo(() => {
    const statusH = 32 + 2;
    return profile.logicalH - profile.headerH - statusH - profile.softBtnH;
  }, [profile]);

  const jog = (dir: 1 | -1) => {
    setAxes((prev) =>
      prev.map((a, i) =>
        i === activeAxis
          ? { ...a, value: Math.round((a.value + dir * step) * 1000) / 1000 }
          : a
      )
    );
  };

  // Use CSS zoom (not transform:scale) so hit-testing matches the painted
  // pixels — transform leaves the layout box at 1× and breaks clicks.
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
        {/* Header tabs */}
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

        {/* Content */}
        <div
          className="relative overflow-hidden bg-[#404040] px-1.5 py-1"
          style={{ height: contentH }}
        >
          {page === "MPG" && (
            <MpgView
              axes={axes}
              activeAxis={activeAxis}
              step={step}
              onSelectAxis={setActiveAxis}
              onStep={setStep}
              profile={profile}
            />
          )}
          {page === "OVERRIDE" && (
            <OverrideView
              feed={feedOverride}
              onChange={setFeedOverride}
            />
          )}
          {page !== "MPG" && page !== "OVERRIDE" && (
            <PlaceholderView title={page} profile={profile} />
          )}
        </div>

        {/* Status + MPG */}
        <div className="flex items-stretch border-t border-black/40 bg-[#555]">
          <div className="flex min-w-0 flex-1 flex-col justify-center px-2 py-1">
            <div className="text-[13px] font-bold leading-none text-white">
              Idle
            </div>
            <div className="mt-0.5 truncate text-[9px] text-zinc-200">
              WPos · mm · {profile.shortName}
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

        {/* Soft buttons */}
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
  onSelectAxis,
  onStep,
  profile,
}: {
  axes: Axis[];
  activeAxis: number;
  step: number;
  onSelectAxis: (i: number) => void;
  onStep: (s: number) => void;
  profile: ScreenProfile;
}) {
  const steps = [0.01, 0.1, 1, 10];
  const roomy = profile.logicalW >= 480;

  return (
    <div className="flex h-full flex-col gap-1">
      {axes.map((axis, i) => (
        <button
          key={axis.name}
          type="button"
          onClick={() => onSelectAxis(i)}
          className={cn(
            "flex items-center justify-between rounded-sm border px-2 transition-colors",
            roomy ? "py-3" : "py-2",
            i === activeAxis
              ? "border-amber-400 bg-[#1f1f1f]"
              : "border-[#222] bg-[#333]"
          )}
        >
          <span
            className={cn(
              "font-bold text-amber-300",
              roomy ? "text-xl" : "text-base"
            )}
          >
            {axis.name}
          </span>
          <span
            className={cn(
              "tabular-nums text-white",
              roomy ? "text-2xl" : "text-lg"
            )}
          >
            {axis.value.toFixed(3)}
          </span>
        </button>
      ))}

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

function OverrideView({
  feed,
  onChange,
}: {
  feed: number;
  onChange: (n: number) => void;
}) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-3">
      <div className="text-[10px] uppercase tracking-widest text-zinc-300">
        Feed override
      </div>
      <div className="text-4xl font-bold tabular-nums text-white">{feed}%</div>
      <div className="flex gap-2">
        {[-10, -1, 1, 10].map((d) => (
          <button
            key={d}
            type="button"
            onClick={() =>
              onChange(Math.min(200, Math.max(10, feed + d)))
            }
            className="min-w-12 rounded-sm border border-[#222] bg-[#2f2f2f] px-3 py-2 text-sm font-semibold text-white"
          >
            {d > 0 ? `+${d}` : d}
          </button>
        ))}
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
