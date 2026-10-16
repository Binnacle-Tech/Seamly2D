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
| `src/app/seamly2d/mainwindow.cpp` | one include and one `Seamly3D::Workspace3DIntegration::install(...)` call, which passes `LoadPattern` so imported patterns open in the main window |
| `src/test/test.pro` | adds the `Seamly3DTest` subdir |

`Workspace3DIntegration` finds the host's File, View and Help menus at runtime
by object name (`file_Menu`, `view_Menu`, `help_Menu`), so `mainwindow.ui`
doesn't change.
**View > 3D Workspace** shows the workspace as a dock of the main window,
beside the 2D pattern. You can float it out to its own window or screen.

`patternsource.cpp` is the only 3D file that reads 2D data. It reads the open
pattern through the application's current data container (pieces,
measurements) and document (change notifications). It never writes to them.

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
| `PatternSource` / `PatternProvider` | Read-only access to the open pattern; the window takes any provider, so tests can feed it pieces. |
| `PieceMesher`, `PieceArrangement`, `PatternPiecesPanel` | Pieces turned into meshes, placed around the body, and the panel to arrange them. |
| `GarmentPanel`, `Seam`, `SeamFinder` | The pieces in 3D with their edges (between the piece's own points), and the seams found between those edges. |
| `ClothSimulator`, `BodyCollider`, `FabricPreset` | Sews the pieces together and drapes them on the body (XPBD), keeping the cloth off the body and the floor, in the chosen fabric. Gives the stretch and ease of the draped garment. |
| `FitMap` | Colors for the fit maps and their legend. |
| `MeasurementGuideWidget` / `MeasurementGuideDialog` | Where to take each measurement, using Seamly2D's diagrams. |
| `Workspace3DIntegration` | The single hook into the 2D main window. |
| `DxfReader`, `SvgReader`, `DxfPatternReader` (`dxfimport/`) | Reads ASCII DXF (blocks, polylines with arcs, splines, ellipses, text) and SVG (paths, shapes, transforms, `<use>`, text, real-world units), and finds the pattern pieces: AAMA/ASTM layers, or closed outlines. |
| `OutlineFitter`, `DxfPatternConverter`, `DxfImportWizard` (`dxfimport/`) | Fits outlines with corners, lines and smooth curves, writes them as a parametric Seamly2D pattern (`.val`), and the wizard around it. |
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

### Pattern pieces

The **Pattern Pieces** tab lists the pieces of the open pattern that are in
the layout, and shows them around the body as the starting shape for sewing
and draping:

* Each piece becomes an even triangle mesh (2.5 cm), wrapped without
  stretching around a cylinder just outside the body (front, back, sides),
  or around the arm for sleeves, from the shoulder tip toward the wrist.
  Collars, pockets and other pieces hang flat beside the body.
* Where a piece goes is guessed from its name (front, back, sleeve, collar...,
  also in German, French, Spanish, Dutch and Italian). You can change it per
  piece, or hide the piece.
* **Pair:** a piece cut twice or on the fold gets a mirrored copy, on the
  other side of the center line or on the other arm. **Flip** mirrors the
  piece when its center line is on the other side.
* **Follow pattern changes** updates the pieces while you edit the pattern in
  2D, and keeps each piece's settings.
* **Use Pattern's Measurements** fits the body to the measurement file the
  pattern uses.

### Sewing and draping

**Sew & Drape** in the Pattern Pieces tab sews the pieces placed on the body
together and lets the garment fall onto it. **Reset** goes back to the pieces
around the body; changing the pieces, their places or the body resets too.
The status line says how sewing goes; its tooltip lists the seams found.

* **Seams are found automatically.** A piece's edges run between its own
  points (the points of its main path). Two edges are sewn together when they
  are as long as each other (within 5%), have the same shape (one bulges where
  the other curves in, or both are straight), and lie side by side on the same
  side of the body once arranged: side and shoulder seams, the center line of
  a piece cut on the fold and its mirrored copy, the underarm seam of a
  sleeve, darts. A sleeve cap is set into the front and back armholes beside
  it, with up to 15% ease. Necklines and hems stay open.
* **Simulation:** position based dynamics (XPBD) in small steps, 10 per frame,
  each passing four times over the stretch and seam constraints. Woven
  fabrics keep the lengths of the flat piece; knits stretch. Seams pull their
  two edges together at 30 cm/s with gravity off, then gravity drapes the
  garment. The body pushes the cloth out
  to 4 mm from its surface, with friction; the floor stops whatever falls.
  Each particle weighs as much as the fabric around it.
* It runs a few frames at a time between repaints, so the window stays
  responsive, and stops by itself once the garment has settled.
* **Fabric:** cotton shirting, linen, denim, cotton jersey, silk, fleece or
  leather. Each has its own stretch, stiffness, weight per square meter,
  friction and thickness. Changing it sews and drapes the garment again.
* **Fit map:** colors the draped garment.
  * **Stretch** shows how much longer the fabric is than the flat piece:
    blue where it is pushed together, green where it is relaxed, yellow to
    red where it is pulled tight (8% and more).
  * **Ease** shows how far the cloth is from the body: red where it presses
    on the body, through yellow, to green and blue where it hangs 6 cm or
    more away.

Not there yet: collars and waistbands sewn to necklines (pieces hung beside
the body are not sewn), self collision (cloth folding through itself), fabric
presets, and editing the seams by hand.

### Importing SVG and DXF patterns

**File > Import SVG or DXF Pattern...** converts a pattern drawing into a
parametric Seamly2D pattern and opens it. It reads SVG (FreeSewing and other
pattern sites, Inkscape, Illustrator), AAMA / ASTM D6673 DXF files, which most
pattern CAD systems export (Gerber, Lectra, Optitex, CLO, Seamly2D...), and
any other DXF drawing of pattern pieces. The file's content decides which
reader is used, not its name.

* **What it reads:** one piece per block. Cut line (layer 1), turn points (2),
  curve points (3), notches (4), mirror or fold line (6), grain line (7),
  internal lines (8, 11), drill holes (13), sew line (14), and texts such as
  `Piece Name:` and `Quantity:`. Units come from `UNITS:` in the file, then
  from `$INSUNITS`, otherwise they are guessed from the size of the pieces.
  For SVG and plain DXF, every outermost closed outline is a piece. An outline
  running parallel inside it is its sew line. Lines and curves drawn end to
  end are joined. SVG is scaled to real size from its width, height and
  viewBox (96 pixels per inch without units).
* **Drawings without layers** (SVG, or DXF exported from a PDF) are sorted
  out by shape. Small circles centered on the edge are notches. Isolated
  small circles inside are drill holes (buttons, snaps). The longest straight
  line is the grain line, and a bracket along the edge marks a fold. Letters
  drawn as outlines, text frames, logos and edges drawn twice are left out.
  Real text names the piece and gives its quantity ("Back", "Cut 2 from main
  fabric", "Cut on fold").
* **What it makes:** each piece gets its own draft block. Its first corner is
  a base point, and every other point sits at a length and an angle from the
  point before it (or from the base point, if you choose that). So you can
  edit it, or replace the numbers with formulas, like any drafted pattern.
  Corners are the file's turn points and anywhere the outline turns sharply.
  Long edges stay straight lines. Curves become spline paths through the
  file's curve points, plus as few extra points as it takes to stay within
  0.5 mm of the file (you can change that). Notches, the grain line, quantity,
  fold, internal lines (as internal paths) and drill holes (as anchor points)
  carry over.
* **Seam allowance:** when the file has a sew line, it becomes the seam line,
  and the allowance is measured edge by edge up to the cut line, so a 3 cm
  hem keeps its 3 cm. Without a sew line, the cut line is kept with the
  allowance included. You can instead treat the cut line as the seam line
  and add an allowance.
* **The wizard:** choose the file, check the pieces and their names (with a
  preview of each converted piece), choose units and options, then pick
  where to save. The pattern opens when you're done.
* Binary DXF isn't supported: save it as ASCII DXF first. Patterns split
  across printed pages (tiled PDFs) have to be assembled first.

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
   - [x] Triangulate piece outlines (`PolygonTriangulator`, `PieceMesher`)
   - [x] Read-only adapter that gets the pieces and measurements of the open
     pattern (`PatternSource`)
   - [x] Arrangement around the avatar (front, back, sides, sleeves along the
     arms, pieces beside the body), pairs for pieces cut twice or on the fold
   - [ ] Drag pieces in 3D; use grainlines and notches for orientation
   - [x] Import DXF patterns from other CAD systems (AAMA/ASTM) as
     parametric Seamly2D patterns
3. **Sewing**
   - [x] Seams found automatically between piece edges: one to one (sides,
     shoulders, folds, darts, underarms) and sleeve caps into armholes
   - [ ] Edit seams by hand, drawn on the 2D pieces, with notches as
     alignment hints; collars and waistbands
4. **Cloth simulation**
   - [x] Even triangle mesh of each piece (Delaunay flips, longest edge
     bisection)
   - [x] XPBD solver: stretch, bending, seam constraints, gravity, damping
   - [x] Collision with the avatar (closest point on a triangle grid) and the
     floor, with friction
   - [ ] Self collision; a smoothed collision body for the finest details
   - [x] Fabric presets (cotton, linen, denim, jersey, silk, fleece, leather)
     mapped to stretch, stiffness, weight, friction and thickness
   - [ ] Per piece fabrics, custom fabrics; multithreaded solver
5. **Fit analysis**
   - [x] Stretch map and ease (distance from the body) map, with a legend
   - [ ] Pressure map, girths of the garment measured with `MeshSlicer`
6. **2D ↔ 3D sync**
   - [ ] Edits in the 2D pattern update the 3D garment
   - [ ] Pins, folding and tacking in 3D
7. **Presentation**
   - [ ] Fabric textures and prints from UV coordinates of the 2D pieces,
     better shading, turntable images and video, garment export (OBJ/glTF)
