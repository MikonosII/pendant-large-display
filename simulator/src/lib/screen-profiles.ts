/** Mirrors Application/ScreenConfig.h + CrowPanel Advance targets */

export type ProfileId = 0 | 1 | 2 | 3;

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
    name: '5.0" Advance ST7262 (480×800)',
    shortName: "5.0″",
    diagonalIn: 5.0,
    logicalW: 480,
    logicalH: 800,
    headerH: 56,
    tabsH: 48,
    softBtnH: 48,
    cmakeValue: "2",
    hardwareNote:
      "Default target: CrowPanel Advance 5″ IPS (ST7262) 800×480 → portrait UI 480×800. Fits tighter machine enclosures than the 7″.",
  },
  {
    id: 3,
    name: '7.0" Advance GW1076 (480×800)',
    shortName: "7.0″",
    diagonalIn: 7.0,
    logicalW: 480,
    logicalH: 800,
    headerH: 56,
    tabsH: 48,
    softBtnH: 48,
    cmakeValue: "3",
    hardwareNote:
      "Same 480×800 chrome as 5″ — CrowPanel Advance 7″ (GW1076-i2). Kept in this fork for larger HMIs.",
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
