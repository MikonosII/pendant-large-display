/** @deprecated Use gcode-path + sample-job — kept for import compatibility. */
export type { PathSeg } from "./gcode-path";
export { toolpathBounds, parseGcodeToolpath } from "./gcode-path";
export { SAMPLE_JOB_NC as DEMO_NC } from "./sample-job";

import { parseGcodeToolpath } from "./gcode-path";
import { SAMPLE_JOB_NC } from "./sample-job";

export const DEMO_TOOLPATH = parseGcodeToolpath(SAMPLE_JOB_NC).path;
