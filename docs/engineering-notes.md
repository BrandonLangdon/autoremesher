# Engineering Notes — Formats, Robustness, and the fTetWild Remesh Stage

This document records the decisions, root-cause investigations, and known
limitations from the work that added STL/3MF import, fixed a remesher crash and
a headless hang, and introduced an optional fTetWild remesh stage. It is meant
to save the next contributor from re-deriving why things are the way they are.

_Landed on `master` via the merge "STL/3MF import, multi-island crash fix, and
optional fTetWild remesh."_

---

## 1. Context / motivation

- **Only Wavefront `.obj` could be loaded.** Real source models are commonly STL
  or 3MF (3D-print pipelines), so users were blocked at step zero.
- **The built-in isotropic remesher does not scale.** A clean 1.25M-triangle
  model ("pot") ran the isotropic stage for **18+ hours** without finishing.
- **A latent concurrency bug crashed the remesher** on any mesh that splits into
  more than one island (very common for multi-part models).

---

## 2. Decisions (what and why)

### 2.1 Add STL and 3MF import
- Wired both into the existing loader path so the GUI Open dialog and the
  headless `--input` share one by-extension dispatcher (`loadModelFile`).
- **Vertex welding is mandatory, not optional.** The pipeline (island
  separation, quad_cover) is connectivity-based and assumes *shared* vertex
  indices. STL stores geometry per-triangle with no shared indices at all; 3MF
  and OBJ routinely contain coincident-but-distinct vertices. Without welding,
  every triangle becomes its own island and the parameterization degrades badly.
  Welding uses the existing `PositionKey` (1e-5 quantization).
- **3MF is a ZIP+XML container.** Implemented with a minimal central-directory
  ZIP reader + `zlib` inflate (already linked) + `QXmlStreamReader` — **no new
  dependencies**. Production 3MF (BambuStudio/Orca/Prusa) splits geometry across
  parts: the root `3D/3dmodel.model` is only a container of build items /
  component references, and the actual mesh lives in referenced
  `3D/Objects/*.model` parts. The loader therefore inflates and merges **all**
  `.model` parts, not just the first.

### 2.2 Introduce a robust/scalable remesh stage: fTetWild
- Chose **fTetWild** over the originally-cloned **TetWild**: 10–50× faster with
  the same robustness, and MPL-2.0 without the CGAL/GPL entanglement that makes
  original TetWild GPL-encumbered.
- Chose **subprocess integration over library linking**:
  - Build-system mismatch (AutoRemesher is qmake; fTetWild is CMake).
  - fTetWild bundles its **own geogram**, which would collide with
    AutoRemesher's geogram 1.8.3 if linked into one binary.
  - Licensing: a subprocess boundary keeps AutoRemesher MIT.
- **Opt-in with graceful fallback.** A toggle (`--use-ftetwild` / GUI checkbox)
  plus the `AUTOREMESHER_FTETWILD` env var (or GUI-set path). When enabled and
  configured, the isotropic phase is replaced by a **sequential per-island**
  fTetWild remesh (fTetWild is internally multithreaded, so it saturates cores
  per call). Any island fTetWild fails on falls back to the built-in remesher;
  if no binary is configured, the built-in path runs unchanged.
- **Placement:** fTetWild's extracted boundary surface (`<out>__sf.obj`,
  requested with `--manifold-surface -a <edge length>`) is a clean, watertight,
  uniform-resolution manifold mesh — exactly what quad_cover wants. So it
  *replaces* the isotropic remesh, not the parameterization.

### 2.3 GUI (interim)
- A "Use fTetWild remesher" checkbox + "Set fTetWild binary…" picker in the
  controls panel; the path persists in `Preferences` and is exported via
  `qputenv` so the core locates it. **This is interim** — see Follow-ups.

---

## 3. Challenges and root causes

### 3.1 The multi-island crash (the hard one)
Symptom: heap corruption (`POINTER_BEING_FREED_WAS_NOT_ALLOCATED`,
`vector length_error`) aborting within seconds on any multi-island mesh. The
crash location **moved between runs**, which sent the investigation through two
red herrings before the real cause:

1. **Red herring A — progress reporting.** `AutoRemesher::updateProgress()` is
   called concurrently from the parallel island workers and touched shared state
   + a stdout/Qt-signal callback without a lock. Serialized it with a mutex.
   Real but not the corruptor — the crash simply moved.
2. **Red herring B — `tbbmalloc_proxy`.** The next crash was in the TBB malloc
   zone machinery while the main thread was doing a lazy `dlopen` during AppKit
   startup. `tbbmalloc_proxy` globally hijacks macOS malloc zones and races with
   late `dlopen`. Dropped it from the link (a real hazard; kept out). Crash
   still moved.
3. **Root cause — a data race on a shared `std::vector`.** In the isotropic
   `tbb::parallel_for` over islands, **every worker did `push_back` onto the
   shared `m_isotropicVertices` / `m_isotropicTriangles` preview vectors** with
   no synchronization. Concurrent `push_back` reallocates while another thread
   reads the size → corrupted size → invalid free / length_error, surfacing in
   whichever thread/allocator touched the heap next (hence the moving crash).
   **Fix:** collect the preview mesh **sequentially after** the parallel phase
   (each island context already holds its resampled mesh). 10/10 clean runs on a
   24-island stress mesh afterward (was crash-on-first-run).

Lesson: a moving heap-corruption crash is one data race, not several bugs. The
mutex and proxy changes were legitimate hardening but were not the corruptor.

### 3.2 Headless load-failure hang
`runHeadless()` runs **before** `app.exec()`, so its `QCoreApplication::quit()`
on a failed load was lost and the app idled in the event loop forever (observed:
27-minute non-exit). Fixed by deferring the exit via `QTimer::singleShot` so it
fires once the loop is running, returning a non-zero code.

### 3.3 The 18-hour isotropic hang — size vs geometry
Diagnostics showed the pot is **clean** (watertight, ~0 non-manifold, single
island); the failure is **size** in one island, not bad geometry. grumpycat
(460k tris, also clean, 1 island) finished in 53s, so there is a steep
non-linear wall between ~460k and 1.25M triangles in the isotropic remesher.
This is **not fixed** for the built-in path; fTetWild is the workaround.

### 3.4 Building fTetWild
- Original TetWild via its prebuilt Docker image was unusable for evaluation:
  `linux/amd64` under QEMU on Apple Silicon, ~10+ min for **80k** tris, memory
  capped. Not representative and not a viable integration.
- Native fTetWild build was blocked by **CMake 4.3 removing
  `FetchContent_Populate`**, which the 2020-era bundled libigl relies on. Fixed
  by building with **CMake 3.29** (pre-3.30, where `FetchContent_Populate` still
  works). Requires GMP (`brew install gmp`); native arm64 `FloatTetwild_bin`
  then builds cleanly.

### 3.5 Benchmark that justified the whole thing
Native fTetWild on the full 1.25M-tri pot: **408s** for the remesh (vs 18h+
hang). End-to-end through AutoRemesher (3MF import → fTetWild → quad_cover):
**~12 min total → 19,253 quads** (84% pure quads). Small meshes are *slower* via
fTetWild (fixed overhead: 0.34s built-in vs 34s fTetWild on a 16k sphere), which
is why the stage is opt-in.

---

## 4. Known limitations

- **fTetWild must be built separately.** AutoRemesher only needs the path at
  runtime (`AUTOREMESHER_FTETWILD` or the GUI picker). Native arm64 build needs
  CMake 3.29 + GMP (see §3.4).
- **fTetWild is opt-in and not for small/clean meshes** (fixed overhead).
- **3MF build-item transforms are not applied** — objects are read in model
  space. Single-object files are unaffected; multi-object assemblies with
  per-item transforms could be mispositioned (connectivity is still per-object,
  so islands separate correctly).
- **The built-in isotropic remesher still hangs** on very large single islands
  (§3.3). Use fTetWild for those.
- **GUI is interim** (see below).

---

## 5. Follow-ups (requested UX redesign)

Captured from user testing. Items 1, 3, and 4 landed on branch
`gui-workflow-redesign`; item 2 is still open.

1. **Done.** Expose fTetWild parameters in the GUI (envelope ε `-e`, ideal edge
   length `-l`, `--coarsen`) via the Preferences dialog, threaded through a new
   `ExternalRemesher::Parameters`. `--manifold-surface` stays forced on because
   the pipeline consumes the extracted `__sf.obj` surface.
2. Better guidance on how the sliders affect results — help pop-ups beyond
   tooltips (Sharp Edge / Smooth Normal / Adaptivity / Target Quads / Edge
   Scaling). **Still open.**
3. **Done.** The fTetWild binary path moved out of the controls panel into a
   File > Preferences dialog (macOS Cmd+, via `PreferencesRole`).
4. **Done.** Explicit multi-step workflow: **Open** only loads + shows source
   stats; **Run fTetWild** is a standalone step whose clean surface becomes the
   working mesh; **Remesh to Quads** is a separate step. The GUI quad remesh no
   longer runs fTetWild in-core (that path is retained only for headless
   `--use-ftetwild`).

### 5.1 Busy indicator (from the same redesign)

The old 2px progress bar was effectively invisible and greyed-out buttons read
as "hung." Replaced with a captioned spinner (`WaitingSpinnerWidget`) over the
viewport that mirrors the live pipeline stage (the pipeline already emitted these
strings via `reportProgressDetailed`; they were being discarded). Color, size,
and backdrop contrast are configurable in Preferences.

**Gotcha (macOS):** the viewport is a `QOpenGLWidget`. A translucent overlay
that is a *sibling* of a GL surface composites its semi-transparent pixels
against black, not the GL content — the scrim vanished and the ring rendered
muddy grey. Fix: the spinner is a **top-level** frameless translucent window,
positioned over the viewport and tracked on move/resize. Top-level windows are
composited by the OS and render correctly over OpenGL.
