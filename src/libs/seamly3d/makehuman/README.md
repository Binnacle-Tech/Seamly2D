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
| `plugins/0_modeling_a_measurement.py`: bisection in `MeasurementValueConverter` | `MakeHumanFitter`: bisection for the macros; the measure modifiers are solved together with Newton steps; height scaling |

## The data pack

`share/makehuman_body.smh` was built from MakeHuman commit
`a8bc2d54ff0ac92e78ff71431b1023eda42bf482` (2024-06-26) with

    python3 tools/build_makehuman_pack.py <makehuman checkout> share/makehuman_body.smh

It holds the body part of `data/3dobjs/base.obj` and the adult ("young")
universal, ethnic, height and breast macro targets, plus all `measure`
targets: 13380 vertices, 13378 quads, 172 targets, 4.4 MB. Children,
elderly bodies, proportions and the many detail targets (face, hands...)
are left out to keep the pack small. Re-run the script with more targets to
add them.

SHA-256 of the pack: `b597ba0c40ffb4e5653d5a3dc41f7d20372e8ad9240580c4cb1642dfd66db55b`
