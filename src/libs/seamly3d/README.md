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

Unit tests for everything except the GL widgets are in `src/test/Seamly3DTest`.

## Body from measurements

Enter your measurements in the **Body Measurements** panel, or load your
SeamlyMe `.vit` file. The body in the viewport updates as you type.

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

Known limitations of this first version: it is a dress-form-like mannequin,
not an organic body. Joins at the shoulders and crotch are visible, there are
no feet or fingers, and front/back and left/right asymmetry, posture and bust
shape are not modeled. The roadmap covers these.

## Body models and MakeHuman

[MakeHuman](https://github.com/makehumancommunity/makehuman) is a useful
reference for parametric human models. It also exports OBJ files, which
`ObjMeshReader` imports. Keep the licences in mind:

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
   - [ ] Organic base mesh morphed to the measurements (CC0 MakeHuman base mesh
     as an option), smooth joins, feet and hands
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
