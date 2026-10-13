; SmartPendant sample laser job (LightBurn-style GRBL)
; Badge outline + diamond + cross — units mm, absolute
G90
G21
G0 X5 Y5
M4
; Outer rectangle @ 65% power
G1 X5 Y5 S650 F1200
G1 X75 Y5
G1 X75 Y45
G1 X5 Y45
G1 X5 Y5
M5
G0 X40 Y10
M4
; Inner diamond
G1 X55 Y25 S500 F900
G1 X40 Y40
G1 X25 Y25
G1 X40 Y10
M5
G0 X30 Y25
M4
; Center cross
G1 X50 Y25 S400 F800
G0 X40 Y15
G1 X40 Y35
M5
G0 X0 Y0
; End
