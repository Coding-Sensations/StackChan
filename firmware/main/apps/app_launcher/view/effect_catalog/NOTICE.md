# Source and modifications

This integration preserves341stable catalog IDs and adapts selected routines and the drawGear helper from Coreymillia/341-Xscreensaver-Esp32-24232028-, commit337adf7f983b20a889e6c839696584142451a6e8.

Original source SHA-256:9aa4dfc0a71ca21ddbaf1c2ef83341c5d8bd20cb0ec5ad8c42d88cfb7489486e8.

Copyright (c)2024 ESP32 Screensaver Universe Project. Complete upstream MIT license and additional XScreensaver attribution retained in LICENSE.upstream. Source snapshot retained in upstream-source.txt. No M5Core-specific wrapper copied.

Local modifications: portable RGB565 clipped raster drawing; Arduino drawing/random helper replacement; remove blocking delays; lift function-local static state into per-effect namespace and reset it explicitly (including initialized arrays); saturate byte-color conversions and Lyapunov intensity before floating-point conversion. Initialize switch-assigned color locals to BLACK for a defined fallback. Original effects remain separately named functions. Text uses an original compact3x5 uppercase/digit font; rounded rectangles currently use square corners.

Private source candidate only. The upstream notice refers to original XScreensaver components with MIT/GPL licenses. A component-by-component license/provenance mapping remains required before redistribution; this file does not certify that mapping.

Current selection policy removes 128 entire implementations: the prior 125 light/flashing effects and red/blue full backgrounds (including ChessModels), plus Moire (saved slide ID 226 / catalog index 225) after a reported device freeze, and Hydrostat (saved ID 157 / catalog index 156) and Metaballs (saved ID 220 / catalog index 219) at the owner's request to reduce firmware size. Their stable IDs remain unavailable tombstones; 213 catalog routines plus DVD remain. Moire2 (saved slide ID 225) is a distinct effect and remains available. Intentional neutral dark gray is retained. The immutable upstream reference snapshot is provenance only and is not compiled. The generator reads the same exclusion policy and cannot regenerate removed implementations.

The validation.json file records the original integration benchmark; it does not describe the current filtered catalog or certify device timing.


## Michael-reported hidden list (source-only successor)

74 owner-confirmed catalog matches are pruned in addition to Hydrostat and Metaballs. IDs and
341 slots remain stable tombstones. No live device list was independently read; the
owner supplied the names. Michael explicitly confirmed Buildlwo, FpsGl and Rorschach for the raw spellings
buildiwo, fpsgi and roschach. Those three are now pruned too. These are procedural draw/reset/state
implementations, not similarly named host/system modules. Original upstream source
and license remain unchanged for later restoration. Exact names and saved IDs:

- Braid: saved ID 3, catalog index 2
- Bumps: saved ID 8, catalog index 7
- Cage: saved ID 9, catalog index 8
- CCurve: saved ID 11, catalog index 10
- Celtic: saved ID 12, catalog index 11
- Circuit: saved ID 15, catalog index 14
- CityFlow: saved ID 16, catalog index 15
- Crackberg: saved ID 34, catalog index 33
- Cubenetic: saved ID 39, catalog index 38
- Cuboctaversion: saved ID 44, catalog index 43
- Dangerball: saved ID 47, catalog index 46
- Demon: saved ID 55, catalog index 54
- Dnalogo: saved ID 62, catalog index 61
- Doubletime: saved ID 64, catalog index 63
- DpmsProtocol: saved ID 66, catalog index 65
- DymaxionmapCoords: saved ID 70, catalog index 69
- Easing: saved ID 71, catalog index 70
- Engine: saved ID 74, catalog index 73
- Etruscanvenus: saved ID 80, catalog index 79
- ExtrusionScrew: saved ID 88, catalog index 87
- ExtrusionTwistoid: saved ID 90, catalog index 89
- Exts: saved ID 91, catalog index 90
- ExtSessionLock: saved ID 92, catalog index 91
- FfmpegOut: saved ID 94, catalog index 93
- Filmleader: saved ID 96, catalog index 95
- Flag: saved ID 98, catalog index 97
- Flame: saved ID 99, catalog index 98
- Flipflop: saved ID 100, catalog index 99
- Flipscreen3d: saved ID 101, catalog index 100
- Fliptext: saved ID 102, catalog index 101
- Flow: saved ID 103, catalog index 102
- Fluidballs: saved ID 104, catalog index 103
- Flurry: saved ID 105, catalog index 104
- FlurrySmoke: saved ID 106, catalog index 105
- FlurryStar: saved ID 108, catalog index 107
- FlurryTexture: saved ID 109, catalog index 108
- Flyingtoasters: saved ID 110, catalog index 109
- Fps: saved ID 114, catalog index 113
- Fuzzyflakes: saved ID 116, catalog index 115
- Gallant12x22Bsd: saved ID 118, catalog index 117
- Gears: saved ID 119, catalog index 118
- Geodesicgears: saved ID 121, catalog index 120
- GlutStroke: saved ID 133, catalog index 132
- GlutSwidth: saved ID 134, catalog index 133
- GrabXimage: saved ID 136, catalog index 135
- Gravitywell: saved ID 138, catalog index 137
- Handsy: saved ID 142, catalog index 141
- PacmanLevel: saved ID 237, catalog index 236
- PasswdKerberos: saved ID 240, catalog index 239
- PasswdPwent: saved ID 242, catalog index 241
- Penetrate: saved ID 245, catalog index 244
- Quickhull: saved ID 252, catalog index 251
- Remote: saved ID 257, catalog index 256
- Robot: saved ID 260, catalog index 259
- RobotWireframe: saved ID 261, catalog index 260
- Rocks: saved ID 262, catalog index 261
- SkullModel: saved ID 269, catalog index 268
- SphereversionCorrugations: saved ID 274, catalog index 273
- Splitflap: saved ID 275, catalog index 274
- Starwars: saved ID 279, catalog index 278
- Substrate: saved ID 282, catalog index 281
- Surfaces: saved ID 284, catalog index 283
- Teapot: saved ID 289, catalog index 288
- Tronbit: saved ID 298, catalog index 297
- Twang: saved ID 301, catalog index 300
- Vines: saved ID 307, catalog index 306
- Whirlwindwarp: saved ID 311, catalog index 310
- Xspirograph: saved ID 322, catalog index 321
- Zoom: saved ID 324, catalog index 323
- Binaryhorizon: saved ID 335, catalog index 334
- Blitspin: saved ID 338, catalog index 337

- Buildlwo: saved ID 7, catalog index 6 (explicit spelling confirmation)
- FpsGl: saved ID 115, catalog index 114 (explicit spelling confirmation)
- Rorschach: saved ID 264, catalog index 263 (explicit spelling confirmation)
