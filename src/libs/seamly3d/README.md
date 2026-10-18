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
| `MeshSlicer` | Cuts a mesh with a level plane, like a tape measure, and returns the girth. Proves the fit, finds the legs, neck and waist of the body shown for arranging pieces, and is the basis for measuring garments later. |
| `BodyMeasurementsPanel` | Form for your own measurements (cm or inch), or load them from a `.vit` file. |
| `Mesh`, `Bounds3D` | Indexed triangle mesh, normals, bounds, interleaved GPU data. |
| `ObjMeshReader` / `ObjMeshWriter` | Wavefront OBJ import of scanned or modeled bodies, and export of the generated body. |
| `PolygonTriangulator` | Ear-clipping triangulation of 2D piece outlines, plus flat piece meshes. |
| `OrbitCamera` | Y-up turntable camera: orbit, pan, zoom, frame bounds. |
| `Viewport3D` | `QOpenGLWidget` that draws the shaded body, the pieces or the draped garment (colored per piece or by a fit map), seam lines and a ground grid. Left drag orbits, Shift/right/middle drag pans, the wheel zooms, Ctrl + drag moves a piece (picked by casting a ray). |
| `Workspace3DWindow` | The 3D workspace window. It shows the body built from the measurements (rebuilt live as values change, with the girths measured back off the model in the status bar), or an imported OBJ body, and runs the sewing and draping. |
| `PatternSource` / `PatternProvider` | Read-only access to the open pattern; the window takes any provider, so tests can feed it pieces. |
| `PieceMesher`, `PieceArrangement`, `BodyFrame`, `PatternPiecesPanel` | Pieces turned into meshes, placed around the body (torso, arms, legs, waist, neck) by name or by shape, and moved by hand (`PieceAdjustment`); the panel to arrange them, edit the seams and choose the fabric and fit map. |
| `GarmentPanel`, `Seam`, `SeamFinder` | The pieces in 3D with their edges (the piece's sides), and the seams found between those edges: one to one, sleeve caps into armholes, waistbands and collars along waistlines and necklines, an under collar to its upper collar, one edge to several (a yoke to the panels below it). |
| `ClothSimulator`, `BodyCollider`, `FabricPreset` | Sews the pieces together and drapes them on the body (XPBD), keeping the cloth off the body and the floor, in the chosen fabric. Gives the stretch and ease of the draped garment. |
| `FitMap` | Colors for the fit maps and their legend. |
| `MeasurementGuideWidget` / `MeasurementGuideDialog` | Where to take each measurement, using Seamly2D's diagrams. |
| `WorkspaceSetup` | What the workspace keeps for a pattern (placements, seams, fabric, fit map), in a `.seamly3d` file next to it. |
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
  Pockets, cuffs, facings and other pieces hang flat beside the body.
* **Skirts** (front or back, from the waist) wrap around the hips from the
  waist down. **Trouser legs** (left or right leg, front or back) wrap around
  the leg, the crease (the middle of the hem) in front of it or behind it,
  and blend into the hips above the thigh so the crotch curve reaches the
  center line. The side seam goes outside: it is the side the top of the
  piece leans to, the crotch extension sticking out on the other. The legs
  are found on the body shown, by cutting it at the thigh and the knee.
* **Waistbands** and **collars** go round the waist or the neck lengthwise,
  their lower edge where the top of the skirt or trousers, or the neckline,
  is. A band cut once is a whole ring, its ends at the center front; a band
  cut twice or on the fold is half a ring from the center back to the front,
  its mirrored copy the other half (**Flip** turns it end for end). The
  waist and the neck are found on the body shown. A band drawn upright in
  the pattern is turned, its left side on top.
* Where a piece goes is guessed from its name (front, back, sleeve, skirt,
  trouser, pants, waistband, collar, yoke..., also in German, French,
  Spanish, Dutch and Italian). Short words count at the start of a word, so
  "Front outside panel" is a front, not a side, and "Seitenteil" is a side;
  longer ones anywhere ("Topsleeve"). You can change it per piece, or hide
  the piece.
* **Pieces without a name** that says anything ("Piece 7", from a DXF file
  without names) are guessed from their shape: small pieces (pockets, flaps,
  cuffs, yokes) and long upright strips (facings) beside the body, long flat
  strips as waistbands (50 cm and more) or collars, pieces with a domed top
  in the middle as sleeves, pieces 80 cm long and more as trouser legs, the
  rest on the front. Two-piece sleeves have a lopsided cap and are not told
  from body panels by shape; front from back is not guessed either.
* **Several pieces in one place** (the panels of a princess bodice, a
  two-piece sleeve) go side by side around it in the order of the list,
  rather than on top of each other. A **yoke** goes on top of the front or
  back, and the other pieces there hang below it.
* **Moving a piece by hand:** select it in the list and set **Up** (cm),
  **Around** (degrees) and **Turn** (degrees), or hold **Ctrl** and drag it
  in 3D (Ctrl + click selects it). Up and around are along and about the
  axis of its place: the body's for the torso, skirts and bands, the arm's
  for sleeves, the leg's for trouser legs. Dragging along that axis on the
  screen moves the piece along it; dragging across turns it around. Turn
  spins the flat piece as drawn, for pieces drawn sideways. The mirrored
  copy moves the mirrored way, so pairs stay symmetric. **Put Back** returns
  the piece to where its place puts it. Pieces move until the garment is
  sewn; **Reset** to move them again.
* **Pair:** a piece cut twice or on the fold gets a mirrored copy, on the
  other side of the center line, on the other arm or on the other leg.
  **Flip** mirrors the piece when its center line (or, for a trouser leg,
  its side seam) is on the other side.
* **Follow pattern changes** updates the pieces while you edit the pattern in
  2D, and keeps each piece's settings.
* **Use Pattern's Measurements** fits the body to the measurement file the
  pattern uses.

### Sewing and draping

**Sew & Drape** in the Pattern Pieces tab sews the pieces placed on the body
together and lets the garment fall onto it. **Reset** goes back to the pieces
around the body; changing the pieces, their places or the body resets too.
The status line says how sewing goes; its tooltip lists the seams sewn.

* **Seams are found automatically.** A piece's edges are its sides: they run
  between the points where its outline turns sharply (more than 25 degrees),
  so notches and points along a smooth curve do not split them. Two edges are
  sewn together when they are as long as each other (within 5%), have the
  same shape (one bulges where the other curves in, both are straight, or
  both bulge out alike over the hips), and lie side by side on the same side
  of the body once arranged: side and shoulder seams, the center line of a
  piece cut on the fold and its mirrored copy, the underarm seam of a sleeve,
  darts. Two edges that both curve in are sewn only when they already lie
  together: the inseams and crotch seams of trousers, but not armholes or
  necklines. A sleeve cap is set into the front and back armholes beside it,
  with up to 15% ease. Then each waistband or collar is sewn on: its lower
  edge along every edge still open within 20 cm of it and at its height
  (from 12 cm below to 5 cm above: the waistlines, the necklines), in order
  around the body, each edge taking a share of the band as long as itself;
  half a band takes the edges on its side only. A second band lying on the
  first (an under collar on its upper collar) is sewn round to it, edge by
  edge. Last, an edge still open is sewn to the open edges of other pieces
  lying along it (within 5 cm) when they are as long as it together (within
  8%): a yoke's lower edge to the tops of the panels below it. Without
  these, necklines and waistlines (a straight top edge) stay open; hems
  always do.
* **Seams list:** every seam, with its color. Uncheck a seam to leave it open;
  **Add Seam...** sews two edges you pick (each listed with its length, and
  shown in yellow in 3D while you choose; the ends that meet are chosen from
  where they lie, and can be swapped), or part of them: **from** and **to**,
  in percent of each edge's length along it; **Remove** deletes a seam you
  added or switches off one that was found; **Find Seams** sews every seam
  found again. Seams you switch off or add are kept when the pieces move.
  Changing the seams sews and drapes again.
* **Sew by Clicking:** while the button is down, click an edge of a piece in
  3D (it turns yellow), then the edge to sew it to; the seam is added, its
  ends joined the way they lie. Go on with the next pair; a click beside the
  pieces starts the pair again; the button again stops. It works on the
  pieces as arranged, before they are sewn.
* **Show seams** draws each seam in 3D as its two edges in its color, with
  ties between the points that meet, on the pieces as arranged and on the
  cloth as it drapes. Selecting a seam in the list shows its edges in yellow.
* **Simulation:** position based dynamics (XPBD) in small steps, 30 per frame,
  each passing four times over the stretch and seam constraints. That many
  steps let a waistline, pressed onto the body, pull itself back up: skirts
  and trousers hang from the waist instead of slipping over the hips (with
  10 steps they slid down). The pieces are solved side by side on all
  processor cores, and so is the collision with the body. Woven
  fabrics keep the lengths of the flat piece; knits stretch. Seams pull their
  two edges together at 30 cm/s with gravity off, then gravity drapes the
  garment. No particle moves faster than 225 cm/s: faster, a sleeve sewn
  round the arm whipped round it and tangled at the hem. The body pushes
  the cloth out to 4 mm from its surface, with friction; the floor stops
  whatever falls.
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

Not there yet: collars that fold over (a collar is a stand, it stays up),
pockets, cuffs and facings (pieces hung beside the body are not sewn), self
collision (cloth folding through itself), a fabric per piece, and picking
seam edges in 3D or on the 2D pieces. A
waistline holding a garment up still stretches a few percent (about 4% for
a skirt and 8% for trousers on the default body) where woven fabric would
not, so it sits a little low.

### The 3D setup is kept with the pattern

What you set up in the 3D workspace is kept in a file next to the pattern,
named after it: `shirt.val` gets `shirt.seamly3d`. The pattern file and its
format do not change, so the pattern still opens in any Seamly2D. The file
keeps:

* where each piece goes, its **Pair** and **Flip** boxes, and how far it was
  moved by hand (a piece is found again by its id, or by its name if the
  pattern was rebuilt);
* the seams you switched off and the ones you added;
* the fabric and the fit map.

It is written half a second after each change, and read when the workspace
opens the pattern. Opening a pattern and changing nothing writes nothing. A
new pattern has no file yet: the setup is kept in the workspace, and written
next to the pattern with the first change after you save it. After **Save
As**, changes go next to the new file. A setup file that cannot be read is
reported in the Pattern Pieces tab, and the pieces keep their guesses.

It is JSON, so it can be read and kept in version control with the pattern:

```json
{
    "fabric": "denim",
    "fitMap": "ease",
    "format": "seamly3d-setup",
    "pieces": [
        {
            "around": -10,
            "flip": true,
            "id": 42,
            "lift": 2.5,
            "name": "Sleeve",
            "pair": true,
            "placement": "right-sleeve",
            "turn": 0
        }
    ],
    "seams": {
        "added": [
            {
                "copyA": 0,
                "copyB": 0,
                "edgeA": 0,
                "edgeB": 0,
                "endA": 1,
                "pieceA": 40,
                "pieceB": 41,
                "reversed": false,
                "startA": 0
            }
        ],
        "off": [
            "40:0:1-41:1:1@0.000"
        ]
    },
    "version": 1
}
```

Placements are named (`front`, `back`, `left-side`, `right-side`,
`left-sleeve`, `right-sleeve`, `lower-front`, `lower-back`,
`left-leg-front`, `left-leg-back`, `right-leg-front`, `right-leg-back`,
`waistband`, `collar`, `aside`, `hidden`), a piece moved by hand has
`lift` (cm), `around` and `turn` (degrees), a seam has the part of each edge
it takes (`startA`, `endA`, `startB`, `endB`, shares of the edge from 0 to
1), and a seam left open is named
`pieceA:copyA:edgeA-pieceB:copyB:edgeB@startA`, edges counted from 0, with
`/startB` after it when only part of edge B is sewn. A
file from a newer version is read as far as it is understood.

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

### Next up

In order, because each one unblocks real patterns rather than test ones:

1. **Imported outlines in fewer, longer edges:** a curve traced from an SVG
   or DXF file breaks into many short edges where it turns sharply between
   points (FreeSewing's Devon yoke has 29), so its lower edge no longer
   lines up with the panels below it. Together with notches (from the
   pattern) to line seams up and to split edges where the pattern does.
2. **Fabric grain:** stretch that differs along the warp, the weft and the
   bias, from the grainline, and a fabric per piece.
3. **Self collision**, which layered garments, fold-over collars and
   tucked-in shirts need.
4. **Arrange from the pattern itself:** use grainlines and notches to tell
   which way a piece goes, two-piece sleeves and front from back by shape,
   and pieces wider than their place (Devon's three front panels spread
   past the side seam).

### 1. Avatar

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
- [ ] Save and load bodies (measurements plus build) apart from a pattern

### 2. Pattern pieces in 3D

- [x] Triangulate piece outlines (`PolygonTriangulator`, `PieceMesher`)
- [x] Read-only adapter that gets the pieces and measurements of the open
  pattern (`PatternSource`)
- [x] Arrangement around the avatar (front, back, sides, sleeves along the
  arms, skirts from the waist, trouser legs around the legs, waistbands and
  collars, pieces beside the body), pairs for pieces cut twice or on the
  fold
- [x] Import DXF and SVG patterns from other CAD systems and pattern sites
  as parametric Seamly2D patterns
- [x] Move pieces by hand: up, around and turn, in the panel or with
  Ctrl + drag in 3D; kept in the setup file
- [x] Guess placements from shape for pieces without a name; several pieces
  in one place side by side; yokes on top
- [ ] Bend pieces in 3D, and arrangement points to snap them to (as in CLO)
- [ ] Use grainlines and notches for orientation: which way is up, which
  edge is the center line or the side seam (now guessed from the shape,
  with **Flip** and **Turn** to correct it)
- [ ] Tell two-piece sleeves, and fronts from backs, by shape
- [ ] Bands drawn upright in the pattern: tell their top from their bottom
- [ ] Place cuffs (round the wrist), pockets and facings
- [ ] Several garments at once, in layers (a shirt under trousers)

### 3. Sewing

- [x] Seams found automatically between piece edges: one to one (sides,
  shoulders, folds, darts, underarms, inseams, crotch seams), sleeve caps
  into armholes, waistbands and collars along waistlines and necklines
- [x] Seams list: switch seams off, add seams between two edges, seam
  lines drawn in 3D
- [x] Pick seam edges in 3D (Sew by Clicking)
- [x] Sew part of an edge, of either side (Add Seam, in percent)
- [x] One edge sewn to several (a yoke to the panels below it), and an
  under collar to its upper collar
- [ ] Pick seam edges on the 2D pieces too; sew part of an edge between
  notches or points, and line seams up by their notches
- [ ] Fewer, longer edges for outlines traced from imported files
- [ ] Gathers and ease: sew a longer edge into a shorter one on purpose
- [ ] Cuffs, pockets and facings sewn on; collars that fold over
- [ ] Openings and closures: button stands that overlap, flies, zips
- [ ] Fold lines and internal lines as creases (pleats, pressed folds)

### 4. Cloth simulation

- [x] Even triangle mesh of each piece (Delaunay flips, longest edge
  bisection)
- [x] XPBD solver: stretch, bending, seam constraints, gravity, damping
- [x] Collision with the avatar (closest point on a triangle grid) and the
  floor, with friction
- [x] Fabric presets (cotton, linen, denim, jersey, silk, fleece, leather)
  mapped to stretch, stiffness, weight, friction and thickness
- [x] Solver on all processor cores (pieces side by side); 30 steps per
  frame so waistbands hold; a speed limit so sleeves do not tangle
- [ ] Self collision, and collision between garments
- [ ] Waistlines that hold without stretching (now 4% for a skirt, 8% for
  trousers): a stiffer solver for long edges under load
- [ ] Fabric grain: different stretch along the warp, the weft and the bias
- [ ] A fabric per piece, custom fabrics, kept in the setup file
- [ ] A smoothed collision body for the finest details
- [ ] Faster: drape a garment in about a second (now 60 to 90 ms per frame
  on four cores for 4,000 to 6,000 particles, so 3 s of draping takes 5 to
  8 s); finer meshes where the cloth folds

### 5. Fit analysis

- [x] Stretch map and ease (distance from the body) map, with a legend
- [ ] Pressure map
- [ ] Girths of the garment measured with `MeshSlicer`, beside the body's,
  as an ease table (bust, waist, hip, thigh...)
- [ ] Warnings where the garment is too tight to put on or too short

### 6. 2D ↔ 3D sync

- [x] **Follow pattern changes**: the pieces update while the pattern is
  edited in 2D
- [ ] Keep the drape when the pattern changes, instead of starting over
- [ ] Select a piece or an edge in 2D and see it in 3D, and back
- [ ] Pins, folding and tacking in 3D
- [ ] From 3D to 2D: flatten a surface drawn on the body into a piece

### 7. Saving and workflow

- [x] The 3D setup (placements, seams, fabric, fit map) kept next to the
  pattern in a `.seamly3d` file, without changing the `.val` format
- [x] Pieces moved by hand kept in the setup too
- [ ] The body (which model, build) kept in the setup too
- [ ] Undo and redo in the 3D workspace
- [ ] Keep the window layout, the camera and the last fabric between
  sessions

### 8. Presentation

- [ ] Fabric textures and prints from UV coordinates of the 2D pieces
- [ ] Better shading: thickness at the edges, seams and topstitching
- [ ] Turntable images and video
- [ ] Garment export (OBJ, glTF), with or without the body

### 9. Quality, platforms and documentation

- [x] Unit tests for everything but the GL widgets (`Seamly3DTest`), run by
  the Linux CI with `make check`
- [ ] Check the Windows (MSVC) and macOS builds of `seamly3d` in CI, and run
  the tests there
- [ ] Translations: the workspace's texts are not in
  `share/translations/translations.pro` yet; add them there (or in a file of
  their own, so as not to change the main project's)
- [ ] Test with real patterns: Seamly2D samples and imported FreeSewing
  patterns, draped and checked by eye and by numbers
- [ ] A user guide with pictures, in the Seamly2D documentation
