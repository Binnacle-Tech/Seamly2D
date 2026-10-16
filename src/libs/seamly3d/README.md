# Seamly3D – 3D workspace

`seamly3d` is a self-contained static library that holds the 3D workspace of
Seamly2D. Per `AGENTS.md`, it lives next to the 2D code, not inside it, so
fixes from the main Seamly2D repository still merge cleanly.

## Integration footprint

These are the only changes to existing files:

| File | Change |
|------|--------|
| `src/libs/libs.pro` | adds the `seamly3d` subdir |
| `src/app/seamly2d/seamly2d.pro` | `QT += opengl openglwidgets` and links `libseamly3d` |
| `src/app/seamly2d/mainwindow.cpp` | one include and one `Seamly3D::Workspace3DIntegration::install(this)` call |
| `src/test/test.pro` | adds the `Seamly3DTest` subdir |

`Workspace3DIntegration` finds the host's View menu at runtime by object name
(`view_Menu`) and adds a **3D Workspace** action to it, so `mainwindow.ui`
doesn't change.

## Dependencies

Only Qt modules: `QtGui`, `QtOpenGL` and `QtOpenGLWidgets`, which ship with
`qtbase`. Qt3D isn't used, so CI and installers need no extra Qt modules. The
shaders are GLSL 1.10 / GLSL ES 1.00, so they run with the default OpenGL 2.1
context on Windows, macOS and Linux.

## Components

| Class | Purpose |
|-------|---------|
| `BodyMeasurements` | The measurements behind the body, keyed by Seamly2D measurement names (`bust_circ`, `height_waist_side`, ...). Has defaults, rescales to a height, sanitizes bad input, and reads SeamlyMe `.vit` files in mm, cm or inch. |
| `ParametricBody` | Builds a body (torso + head, legs, arms) that fits the measurements. Girths are exact. |
| `MeshSlicer` | Cuts a mesh with a level plane, like a tape measure, and returns the girth. Proves the fit and is the basis for measuring garments later. |
| `BodyMeasurementsPanel` | Form for your own measurements (cm or inch), or load them from a `.vit` file. |
| `Mesh`, `Bounds3D` | Indexed triangle mesh, normals, bounds, interleaved GPU data. |
| `ObjMeshReader` / `ObjMeshWriter` | Wavefront OBJ import of scanned or modeled bodies, and export of the generated body. |
| `PolygonTriangulator` | Ear-clipping triangulation of 2D piece outlines, plus flat piece meshes. This is the first step toward putting pattern pieces into 3D. |
| `OrbitCamera` | Y-up turntable camera: orbit, pan, zoom, frame bounds. |
| `Viewport3D` | `QOpenGLWidget` that draws the shaded mesh and a ground grid. Left drag orbits, Shift/right/middle drag pans, the wheel zooms. |
| `Workspace3DWindow` | The 3D workspace window. It shows the body built from the measurements (rebuilt live as values change, with the girths measured back off the model in the status bar), or an imported OBJ body. |
| `Workspace3DIntegration` | The single hook into the 2D main window. |

| `MakeHumanData`, `MakeHumanBody`, `MakeHumanFitter` (`makehuman/`) | Realistic body: MakeHuman's base mesh and targets, fitted to the measurements. AGPL port, see `makehuman/README.md`. |

Unit tests for everything except the GL widgets are in `src/test/Seamly3DTest`.

## Body from measurements

Enter your measurements in the **Body Measurements** panel, or load your
SeamlyMe `.vit` file. The body in the viewport updates as you type. The
toolbar chooses between two body models.

### Realistic (MakeHuman), the default

Every measurement in the panel is fitted: all heights, all girths, shoulder
width, arm length and head circumference.

* **Build:** body shape (female/male), muscle tone and age (1 to 90 years) come
  from the panel. The age can also come from the `.vit` file's birth date.
* **Overall shape:** MakeHuman's height, weight and (for women) cup size
  macros are found by bisection. A uniform scale then makes the height exact.
* **Every measurement together:** the fitter solves all modifiers at once
  with damped least squares (Levenberg-Marquardt), comparing measurements
  taken by `MakeHumanTape`. The modifiers are MakeHuman's measure modifiers,
  the modeling targets for leg, hip, torso, neck and head proportions, arm fat,
  and a shoulder-slope target generated from the skeleton weights.
* **Natural over extreme:** each modifier pays a small cost for moving, and
  landmark heights count half as much as girths. Measurements that agree with
  a human body fit within about 0.1 cm (girths) and 0.3 cm (heights).
  Inconsistent ones are matched as far as a natural body allows, without
  creases. The status bar shows the largest difference, and its tooltip has
  every measurement, wanted and achieved.
* **Children** (under 13) get a flat chest. Lowbust and bustpoint are used
  only for women: they describe a bust, and on children and men MakeHuman's
  lowbust tape carves a rib line into the chest.
* **Changing the height** scales every measurement by the same factor (with
  "Scale everything with height" on), so you keep the same body shape at the
  new size. Enter your height first, then adjust what differs.
* **Defaults** are MakeHuman's average woman (168 cm) or man (178 cm).
  Switching the body shape while the measurements are still the defaults
  switches to the other set, and **Reset** uses the current shape's defaults.
* **Speed:** a fit takes 0.15 to 0.6 s in a release build. It runs in the
  background, so typing stays responsive.

How `MakeHumanTape` measures, following the Seamly2D definitions:

| Measurement | On the body |
|-------------|-------------|
| Heights | Floor to: the nape, shoulder tips, armpit (where a level cut first separates the arm), bust point (most forward point of the bust), side of the waist and hip loops, crotch (where the legs separate), level of the knee, calf and ankle loops |
| Neck | Around the neck base, touching the nape, the neck sides and the front of the neck base |
| Bust, lowbust, waist, hip, thigh, knee, calf, ankle, wrist | MakeHuman's tape loops |
| Upper arm | Cut square to the upper arm just below the armpit |
| Elbow | Cut square to the forearm through the elbow |
| Head | Level cut through its widest part |
| Shoulder width | Shoulder tip to shoulder tip along the back |
| Arm length | Shoulder tip to elbow to wrist (MakeHuman's arm rulers) |

The default measurements are MakeHuman's average woman (25 years), measured
this way and scaled to 168 cm, so the default body needs no reshaping. When
you change only the height, the other lengths follow real proportions.

### Simple mannequin

How it works:

* Each landmark in the measurement database (bust, lowbust, waist, hip, neck,
  head, thigh, knee, calf, ankle, and the arm girths) becomes an elliptical
  cross section at the measured height. The section is scaled so its
  perimeter equals the measured girth, so a tape measure on the model gives
  your measurement back. `Seamly3DTest` checks this for several body types.
* Between landmarks, and for things a tape can't measure (depth-to-width
  ratios, the head, the hands), the shape follows average proportions.
* A measurement left out keeps the default, scaled to the person's height.
  Impossible input, such as a waist above the bust, is corrected so a body
  can always be built.
* The arms hang in an A-pose and open just enough to clear the bust.

Known limitations of the mannequin: it is dress-form-like, not an organic
body. Joins at the shoulders and crotch are visible, there are
no feet or fingers, and front/back and left/right asymmetry, posture and bust
shape are not modeled. The roadmap covers these.

### How to measure

The **How to measure** box under the measurement fields shows the Seamly2D
diagram and description of the measurement being edited. The number in
brackets, such as G04, is the label on the diagram. **All Measurements...**,
**Help > How to Measure** in the 3D window (F1), and **Help > How to Measure
(Body)** in the main window open a guide with every measurement.

## Body models and MakeHuman

[MakeHuman](https://github.com/makehumancommunity/makehuman) is a useful
reference for parametric human models. It also exports OBJ files, which
`ObjMeshReader` imports. Keep the licences in mind:

The realistic body is a port of MakeHuman (see `makehuman/README.md`).

* MakeHuman's **code** is AGPL-3.0, and Seamly2D is GPL-3.0-or-later.
  GPLv3 section 13 explicitly allows combining the two, so porting MakeHuman
  code is allowed. Ported files keep their AGPL-3.0 terms and copyright
  notices; keep them in their own files or directory and say so in their
  headers. MakeHuman is written in Python and Seamly2D in C++, so "using its
  code" means porting it (base mesh + morph targets, modifiers, the measure
  plugin), not linking it.
* MakeHuman's **assets** (base mesh, targets) and the models users export
  from it are CC0, so they're fine as test or sample avatars, or as the
  base mesh for a future morphing body.
* MakeHuman exports in decimetres by default, and the parametric body is in
  centimetres. Imported models are shown in their own units for now.

## Roadmap: toward CLO3D / Marvelous Designer features

The goal is the loop those tools are known for: avatar → arrange 2D pieces
around it → sew → simulate → check the fit → edit in 2D and see it in 3D.

1. **Avatar** (started)
   - [x] Body generated from measurements, with exact girths
   - [x] Measurements from SeamlyMe `.vit`, cm or inch
   - [x] OBJ import and export
   - [x] Realistic body: MakeHuman base mesh and targets fitted to every
     measurement in the panel, all ages from 1 to 90
   - [ ] More measurements from the Seamly2D database (highbust, across
     chest/back, waist lengths, crotch length...)
   - [ ] More landmarks: across back and chest, front and back waist length,
     bust point spacing, posture and asymmetry
   - [ ] Units for imported models, and fitting an imported scan to measurements
   - [ ] Poses through a skeleton, so garments can be checked in motion
2. **Pattern pieces in 3D**
   - [x] Triangulate piece outlines (`PolygonTriangulator`)
   - [ ] Read-only adapter that gets piece outlines and seam lines from the open
     pattern, without changing 2D code
   - [ ] Arrangement points around the avatar (front, back, sleeves), with
     pieces wrapped on cylinders as a starting shape
3. **Sewing**
   - [ ] Seams between piece edges (one to one and one to many), drawn on the
     2D pieces, with notches as alignment hints
4. **Cloth simulation**
   - [ ] Remeshing of pieces into an even triangle mesh (particle distance)
   - [ ] XPBD solver: stretch, shear, bending, seam constraints, gravity
   - [ ] Collision with the avatar (signed distance field) and self-collision
   - [ ] Fabric presets (cotton, denim, jersey, silk...) mapped to stiffness,
     weight and friction
5. **Fit analysis**
   - [ ] Strain and stress maps, pressure maps, ease measured with `MeshSlicer`
6. **2D ↔ 3D sync**
   - [ ] Edits in the 2D pattern update the 3D garment
   - [ ] Pins, folding and tacking in 3D
7. **Presentation**
   - [ ] Fabric textures and prints from UV coordinates of the 2D pieces,
     better shading, turntable images and video, garment export (OBJ/glTF)
