M73 P0 R30

M190 S50 ; set bed temperature and wait for it to be reached
M104 S220 ; set temperature
;TYPE:Custom
SET_FAN_SPEED FAN=nevermore1 SPEED=1
SET_FAN_SPEED FAN=nevermore2 SPEED=1
MMU__MOTORS_OFF
MMU ENABLE=0
START_PRINT REFERENCED_TOOLS=!referenced_tools! INITIAL_TOOL=0 EXTRUDER_TEMP=220 BED_TEMP=50

;SET_SKEW XY=AC,BD,AD
SET_SKEW XY=140.8,141.0,99.7

MMU__MOTORS_OFF
MMU ENABLE=0

M109 S220 ; set temperature and wait for it to be reached
G21 ; set units to millimeters
G90 ; use absolute coordinates
M83 ; use relative distances for extrusion
; Filament gcode
SET_GCODE_OFFSET Z=0
SET_PRESSURE_ADVANCE ADVANCE=0.00
SET_PRESSURE_ADVANCE EXTRUDER=extruder SMOOTH_TIME=0.02
M107