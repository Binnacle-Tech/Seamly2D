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
| `Mesh`, `Bounds3D` | Indexed triangle mesh, normals, bounds, interleaved GPU data. |
| `ObjMeshReader` | Wavefront OBJ import for body models (v / vn / f, every face form, negative indices, n-gons). |
| `PolygonTriangulator` | Ear-clipping triangulation of 2D piece outlines, plus flat piece meshes. This is the first step toward putting pattern pieces into 3D. |
| `OrbitCamera` | Y-up turntable camera: orbit, pan, zoom, frame bounds. |
| `Viewport3D` | `QOpenGLWidget` that draws the shaded mesh and a ground grid. Left drag orbits, Shift/right/middle drag pans, the wheel zooms. |
| `Workspace3DWindow` | The 3D workspace window: import or clear a body model, frame or reset the view, toggle the grid. |
| `Workspace3DIntegration` | The single hook into the 2D main window. |

Unit tests for everything except the GL widgets are in `src/test/Seamly3DTest`.

## Body models and MakeHuman

[MakeHuman](https://github.com/makehumancommunity/makehuman) is a useful
reference for parametric human models. It also exports OBJ files, which
`ObjMeshReader` imports. Keep the licences in mind:

* MakeHuman's **code** is AGPL-3.0, and Seamly2D is GPL-3.0. Don't copy
  MakeHuman source into this library. Study its approach (targets and
  modifiers that morph a base mesh, measurement-driven modifiers, the
  skeleton) and write our own implementation.
* MakeHuman's **assets** (base mesh, targets) and the models users export
  from it are CC0, so they're fine to use as test or sample avatars.
* MakeHuman exports in decimetres by default, and Seamly2D works in mm, cm
  or inch. Unit handling (see the roadmap) has to cover this before
  measurements can be compared.

## Roadmap

1. **Units:** choose or detect the avatar's units and convert to the
   pattern's units.
2. **Measurements:** link an avatar to a `.vit`/`.vst` file. Measure girths
   by slicing the mesh with planes, and lengths along the surface.
3. **Pattern pieces in 3D:** take piece outlines (seam lines) from the open
   pattern through a read-only adapter, build flat meshes with
   `PolygonTriangulator`, and place them around the avatar.
4. **Sewing and simulation:** define seams between piece edges, then add a
   simple mass-spring or position-based cloth solver with collision against
   the avatar.
5. **Parametric avatars:** morph a base mesh from measurements, as
   MakeHuman does with its modifiers (our own implementation).
