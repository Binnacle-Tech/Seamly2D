# MakeHuman body for the 3D workspace

This directory holds the realistic body of the 3D workspace. It is a C++
port of parts of [MakeHuman 1.x](https://github.com/makehumancommunity/makehuman),
plus the MakeHuman data it needs.

## Licences

| What | Licence |
|------|---------|
| `makehumanmodel.*`, `makehumanfitter.*` (ported code) | **AGPL-3.0-or-later**, Copyright MakeHuman Team, port by the Seamly2D project |
| `share/makehuman_body.smh` (base mesh and targets) | **CC0 1.0** (MakeHuman assets) |
| `tools/build_makehuman_pack.py`, `makehuman.qrc` | GPL-3.0-or-later, like the rest of Seamly2D |

GPL-3.0 and AGPL-3.0 both allow, in section 13, combining code under the
other licence into one work. The ported files keep the AGPL. Since Seamly2D
is a desktop program, AGPL's extra network-interaction clause doesn't come
into play. If Seamly2D is ever offered to users over a network, the
corresponding source of these files has to be offered to those users.

## What was ported

| MakeHuman source | Port |
|------------------|------|
| `lib/targets.py`: macro categories and how target names map to factors | `MakeHumanData::targetFactors()` |
| `apps/human.py`: `_setGenderVals`, `_setAgeVals`, `_setWeightVals`, ... | `MakeHumanMacros::factors()` |
| `apps/humanmodifier.py`: `getTargetWeights`, decr/incr modifier factors | `MakeHumanBody::setMacros()`, `setModifier()` |
| `plugins/0_modeling_a_measurement.py`: `Ruler` vertex paths and measuring | `MakeHumanBody::measure()` |
| `plugins/0_modeling_a_measurement.py`: bisection in `MeasurementValueConverter` | `MakeHumanFitter`: bisection for the macros (the rest of the fitter is new) |

New, not in MakeHuman:

* `MakeHumanTape` measures every Seamly2D measurement on the body: landmark
  heights, neck base, upper arm at armpit level, elbow, head, shoulder width.
* The fitter solves all modifiers at once with regularized Levenberg-Marquardt,
  instead of one slider at a time.
* Modifiers can go to 1.5 times MakeHuman's slider range.
* The generated shoulder slope target.

## The data pack

`share/makehuman_body.smh` was built from MakeHuman commit
`a8bc2d54ff0ac92e78ff71431b1023eda42bf482` (2024-06-26) with

    python3 tools/build_makehuman_pack.py <makehuman checkout> share/makehuman_body.smh

It holds:

* the body part of `data/3dobjs/base.obj`;
* the macro targets for every age (universal, ethnic, height, breast);
* all `measure` targets;
* the modeling targets the fitter uses: leg lengths, foot height, lower leg,
  hip, waist, torso, neck and head scale, breast height, arm fat;
* `seamly/shoulders-up|down`, made by the script from the default skeleton's
  CC0 skinning weights (clavicle and arm bones), because MakeHuman has no
  shoulder slope target.

In total: 13380 vertices, 13378 quads, 536 targets, 7.0 MB. Index gaps and
separate x, y, z planes make it compress to a third. Proportion targets
and detail targets (face, hands...) are left out.

SHA-256 of the pack: `89e735f6d80062688a361ec918bd8b053caa769f83127c8a513d724494eda3ef`
