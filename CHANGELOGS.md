Unreleased
- Remove the fTetWild integration (GUI "Run fTetWild" step, its Preferences
  settings, --use-ftetwild / AUTOREMESHER_FTETWILD, and the Blender add-on
  option). fTetWild is becoming a separate tool
- macOS build: locate TBB via `brew --prefix tbb` so Intel Macs (/usr/local)
  build, not just Apple Silicon (/opt/homebrew)
- Linux build: only pass -march=x86-64-v2 on x86_64 so aarch64 Linux builds
- Windows: build with Qt 6 (taskbar progress is compiled out when Qt 5's
  winextras module isn't available); Windows CI moves to Qt 6.8.3 / MSVC 2022.
  New step-by-step guide in docs/building-windows.md
- Add a Blender add-on (blender/autoremesher_bridge) that quad-remeshes the
  selected object via the AutoRemesher CLI as a subprocess: replace-in-place or
  new-object output, an object picker, live stage/progress in the status bar,
  and cancel. The core stays standalone; see docs/blender-integration.md
- CLI: include the pipeline stage text in headless progress output, and exit
  non-zero when a headless run produces no geometry

1.1.0
- Import STL (binary/ASCII) and 3MF in addition to OBJ (GUI + CLI), with vertex welding
- Read all model parts from 3MF (production files split geometry into 3D/Objects/*.model)
- Fix a data race that crashed the remesher on any multi-island mesh
- Fix headless CLI hanging instead of exiting on a failed/unsupported load
- Add an optional fTetWild remesh stage (subprocess) for large/messy inputs the
  built-in isotropic remesher cannot handle in reasonable time; opt-in via
  --use-ftetwild / GUI toggle + AUTOREMESHER_FTETWILD, with fallback to built-in
- Rework the GUI into explicit steps: Open now only loads the mesh and shows
  source stats (vertex/triangle counts, bounding-box size) instead of
  auto-remeshing; remeshing is driven by separate "Run fTetWild" and
  "Remesh to Quads" buttons
- Run fTetWild as a standalone GUI step whose clean surface replaces the working
  mesh fed to the quad remesher (the original is kept, so the step is idempotent)
- Move the fTetWild binary path into a File > Preferences dialog (macOS Cmd+,)
  and expose fTetWild parameters there (envelope size, edge length, coarsen)
- Replace the near-invisible progress bar with a captioned busy spinner shown
  over the viewport that reports the live pipeline stage; its color, size, and
  backdrop contrast are configurable in Preferences
- See docs/engineering-notes.md for decisions, root-cause investigations, and limitations

1.0.0
- Relicense from GPLv3 to MIT (reimplemented MIT-incompatible dependencies)
- Improve parameterizer, isotropic remesher, and quad extraction algorithms
- Add adaptivity parameter
- Add sharp edge parameter
- Add smooth normal parameter for low-poly mesh
- Replace density with target quads parameter
- Add command-line interface
- Refine main window, theme, and graphics widgets
- Replace app icon

1.0.0-beta.3
- Remesh isolated meshes separately   
- Improve quad extractor  
- Add edge scaling setting for generating low poly  
- Add rough progress reporting (Windows only)  
- Generate quad dominated mesh    
- Improve parameterization for thin surfaces  

1.0.0-beta.2
- Fix holes  
- Replace Poly budget with density setting  
- Remove laplacian smooth in preprocess  

1.0.0-beta.1
- Replace MIQ with QuadCover  
- Implement simple quad extractor  
- Remove libQEx  
- Add OpenVDB for uniform remeshing  

1.0.0-alpha.4
- Add constrained option: Better Edge Flow/Less Distortion  
- Fix libQEx access violation  
- Fix OpenMesh crash  
- Limit singularities to 320  
- Improve wireframe render  

1.0.0-alpha.3
- Support mesh with holes  
- Generate better edge flow by increase the default constraint ratio from 0.4 to 0.5  
- Alleviate spiral pattern by up-sampling  
- Speed up on complex mesh by reducing singularities   
- Add debug dialog  
