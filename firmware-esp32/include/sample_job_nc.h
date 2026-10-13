#pragma once

// Embedded copy of /jobs/sample-badge.nc — parsed at runtime into the path canvas.
static const char SAMPLE_JOB_NC[] =
    "; SmartPendant sample laser job (LightBurn-style GRBL)\n"
    "; Badge outline + diamond + cross — units mm, absolute\n"
    "G90\n"
    "G21\n"
    "G0 X5 Y5\n"
    "M4\n"
    "; Outer rectangle @ 65% power\n"
    "G1 X5 Y5 S650 F1200\n"
    "G1 X75 Y5\n"
    "G1 X75 Y45\n"
    "G1 X5 Y45\n"
    "G1 X5 Y5\n"
    "M5\n"
    "G0 X40 Y10\n"
    "M4\n"
    "; Inner diamond\n"
    "G1 X55 Y25 S500 F900\n"
    "G1 X40 Y40\n"
    "G1 X25 Y25\n"
    "G1 X40 Y10\n"
    "M5\n"
    "G0 X30 Y25\n"
    "M4\n"
    "; Center cross\n"
    "G1 X50 Y25 S400 F800\n"
    "G0 X40 Y15\n"
    "G1 X40 Y35\n"
    "M5\n"
    "G0 X0 Y0\n"
    "; End\n";

static const char SAMPLE_JOB_NAME[] = "sample-badge.nc";
