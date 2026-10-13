/** Mirrors Application/ScreenConfig.h */

export type ProfileId = 0 | 1 | 2;

export type ScreenProfile = {
  id: ProfileId;
  name: string;
  shortName: string;
  diagonalIn: number;
  logicalW: number;
  logicalH: number;
  headerH: number;
  tabsH: number;
  softBtnH: number;
  cmakeValue: string;
  hardwareNote: string;
};

export const PROFILES: ScreenProfile[] = [
  {
    id: 0,
    name: '3.5" stock (320×480)',
    shortName: "3.5″",
    diagonalIn: 3.5,
    logicalW: 320,
    logicalH: 480,
    headerH: 40,
    tabsH: 40,
    softBtnH: 36,
    cmakeValue: "0",
    hardwareNote:
      "Upstream Devtronic panel: ILI9488 + FT6236 on the BlackPill SPI/I²C bus.",
  },
  {
    id: 1,
    name: '4.0" same resolution',
    shortName: "4.0″",
    diagonalIn: 4.0,
    logicalW: 320,
    logicalH: 480,
    headerH: 40,
    tabsH: 40,
    softBtnH: 36,
    cmakeValue: "1",
    hardwareNote:
      "Physically larger glass at the same 320×480 pixel map — easiest hardware swap.",
  },
  {
    id: 2,
    name: '5.0" large (480×800)',
    shortName: "5.0″",
    diagonalIn: 5.0,
    logicalW: 480,
    logicalH: 800,
    headerH: 56,
    tabsH: 48,
    softBtnH: 48,
    cmakeValue: "2",
    hardwareNote:
      "Default in this fork. Needs an 800×480-class panel (portrait 480×800) and a matching controller — not the stock ILI9488.",
  },
];

export const PAGES = [
  "MPG",
  "OVERRIDE",
  "POWER FEED",
  "GCODE",
  "PROBE",
  "SETTINGS",
] as const;

export type PageId = (typeof PAGES)[number];
