"use client";

import { useState } from "react";
import { Button } from "@/components/ui/button";
import { PendantScreen } from "@/components/pendant/PendantScreen";
import { PROFILES, type ProfileId } from "@/lib/screen-profiles";
import { cn } from "@/lib/utils";

export function LayoutLab() {
  const [profileId, setProfileId] = useState<ProfileId>(2);
  const profile = PROFILES.find((p) => p.id === profileId) ?? PROFILES[2];

  const areaRatio =
    (profile.logicalW * profile.logicalH) / (320 * 480);

  return (
    <div className="mx-auto flex w-full max-w-6xl flex-col gap-8 px-4 py-8 lg:flex-row lg:items-start lg:gap-10 lg:px-8">
      <aside className="w-full shrink-0 space-y-6 lg:w-[340px]">
        <div>
          <p className="text-xs font-semibold uppercase tracking-[0.2em] text-amber-700/90">
            SmartPendant fork
          </p>
          <h1
            className="mt-2 text-5xl leading-none tracking-wide text-zinc-900"
            style={{ fontFamily: "var(--font-bebas), Impact, sans-serif" }}
          >
            Large Screen Lab
          </h1>
          <p className="mt-3 text-sm leading-relaxed text-zinc-600">
            Preview CrowPanel Advance MPG (5″ default, 7″ kept): live map,
            macros, overrides, probe, jobs. Stock 3.5″/4.0″ share the top
            Idle/MPG strip and fill MPG with DRO, 2×4 macros, and steps;
            settings and other tabs stay shared. K+ enable test is physical.
          </p>
        </div>

        <div className="space-y-2">
          <div className="text-xs font-semibold uppercase tracking-wider text-zinc-500">
            Screen profile
          </div>
          <div className="flex flex-col gap-2">
            {PROFILES.map((p) => (
              <button
                key={p.id}
                type="button"
                data-profile={p.id}
                aria-pressed={profileId === p.id}
                onClick={() => setProfileId(p.id)}
                className={cn(
                  "relative z-10 w-full cursor-pointer rounded-md border px-3 py-2.5 text-left transition-colors",
                  profileId === p.id
                    ? "border-amber-600 bg-amber-50 text-zinc-900"
                    : "border-zinc-200 bg-white/70 text-zinc-700 hover:border-zinc-300"
                )}
              >
                <span className="block text-sm font-semibold pointer-events-none">
                  {p.name}
                </span>
                <span className="mt-0.5 block text-[11px] text-zinc-500 pointer-events-none">
                  {p.logicalW}×{p.logicalH} · header {p.headerH}px · soft{" "}
                  {p.softBtnH}px
                </span>
              </button>
            ))}
          </div>
        </div>

        <dl className="grid grid-cols-2 gap-3 rounded-md border border-zinc-200 bg-white/60 p-3 text-sm">
          <div>
            <dt className="text-[10px] uppercase tracking-wider text-zinc-500">
              Pixel area
            </dt>
            <dd className="font-semibold tabular-nums text-zinc-900">
              {areaRatio.toFixed(2)}× stock
            </dd>
          </div>
          <div>
            <dt className="text-[10px] uppercase tracking-wider text-zinc-500">
              CMake flag
            </dt>
            <dd className="font-mono text-xs font-semibold text-zinc-900">
              -DSCREEN_PROFILE={profile.cmakeValue}
            </dd>
          </div>
        </dl>

        <p className="text-xs leading-relaxed text-zinc-500">
          {profile.hardwareNote}
        </p>

        <div className="flex flex-wrap gap-2">
          <Button
            variant="outline"
            size="sm"
            onClick={() => setProfileId(0)}
          >
            Compare stock
          </Button>
          <Button size="sm" onClick={() => setProfileId(2)}>
            Use 5″ Advance
          </Button>
          <Button
            variant="outline"
            size="sm"
            onClick={() => setProfileId(3)}
          >
            Use 7″ Advance
          </Button>
        </div>
      </aside>

      <main className="flex min-w-0 flex-1 flex-col items-center gap-4 overflow-x-auto">
        <div className="rounded-[1.25rem] border border-zinc-700/80 bg-gradient-to-b from-zinc-700 to-zinc-900 p-3 shadow-2xl">
          <div className="mb-2 flex items-center justify-between px-1 text-[10px] uppercase tracking-[0.18em] text-zinc-400">
            <span>SmartPendant</span>
            <span>
              {profile.logicalW}×{profile.logicalH}
            </span>
          </div>
          <PendantScreen profile={profile} />
          <div className="mt-3 flex justify-center">
            <div className="h-2 w-16 rounded-full bg-zinc-600" />
          </div>
        </div>
        <p className="max-w-md text-center text-xs text-zinc-500">
          1:1 logical pixels (no CSS scale — keeps touch targets clickable).
          Firmware uses the same chrome metrics from{" "}
          <code className="text-zinc-700">ScreenConfig.h</code>.
        </p>
      </main>
    </div>
  );
}
