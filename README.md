# AutoRemesher

AutoRemesher is a cross-platform automatic quad remeshing tool that converts high-polygon meshes into clean quad-based topology. It is built on top of libraries: [Geogram](https://github.com/BrunoLevy/geogram), [libigl](https://github.com/libigl), [isotropicremesher](https://github.com/huxingyi/isotropicremesher) and [others](https://github.com/huxingyi/autoremesher/blob/master/ACKNOWLEDGEMENTS.html).

Buy me a coffee for staying up late coding :-) [![](https://www.paypalobjects.com/en_US/i/btn/btn_donate_SM.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=GHALWLWXYGCU6&item_name=Support+me+coding+in+my+spare+time&currency_code=AUD&source=url)

<img width="3644" height="2202" alt="autoremesher-1 0-screenshot" src="https://github.com/user-attachments/assets/47851f1e-127c-49af-81b7-0c8ac06fb3ad" />

## Contents

- [About this fork](#about-this-fork)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building](#building) — [macOS](#macos) · [Linux](#linux-ubuntudebian) · [Windows](#windows-visual-studio-2022)
- [Using the GUI](#using-the-gui)
  - [Workflow](#workflow)
  - [Viewport controls](#viewport-controls)
  - [Parameters](#parameters)
- [Using fTetWild (optional)](#using-ftetwild-optional)
- [Command-line (headless)](#command-line-headless)
- [Quick Start (prebuilt releases)](#quick-start-prebuilt-releases)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## About this fork

This fork ([BrandonLangdon/autoremesher](https://github.com/BrandonLangdon/autoremesher)) adds several improvements aimed at real-world (especially 3D-print) meshes:

- **More input formats** — imports **OBJ, STL** (binary/ASCII), and **3MF** in addition to OBJ, with automatic vertex welding.
- **Step-based GUI workflow** — **Open** now only loads the mesh and shows its stats; remeshing is driven by explicit **Run fTetWild** (optional) and **Remesh to Quads** buttons instead of firing automatically.
- **Optional fTetWild remesh stage** — hand large or messy meshes to [fTetWild](https://github.com/wildmeshing/fTetWild) for a clean, watertight, uniform surface before quad remeshing. See [Using fTetWild](#using-ftetwild-optional).
- **Clear progress feedback** — a captioned spinner over the viewport reports the live pipeline stage instead of a near-invisible progress bar.
- **Trackpad-friendly viewport controls** — plain left-drag orbits, Shift+left-drag pans, and zoom tracks the actual scroll amount.
- **Robustness fixes** — fixes a data race that crashed the remesher on any multi-island mesh, and a headless hang on failed loads.

See [`CHANGELOGS.md`](CHANGELOGS.md) for the full list and [`docs/engineering-notes.md`](docs/engineering-notes.md) for the design decisions and root-cause investigations behind them.

## Getting Started

These instructions will get you a copy of **AutoRemesher** up and running on your local machine for development.

### Prerequisites

- A C++ compiler with **C++17** support (GCC, Clang, or MSVC) — the STL/3MF importer and fTetWild bridge use `std::filesystem`.
- **Qt 5.15.2 or Qt 6.x** (this fork is developed and tested against Qt 6.11).
- **TBB** (Intel Threading Building Blocks).
- **zlib** — used to inflate 3MF archives. Provided by the system on Linux/macOS and bundled on Windows.
- CMake 3.12 or later (only needed on Windows to build TBB from source).

Geogram, Eigen, and the isotropic remesher are vendored under `thirdparty/` — you do not need to install them.

### Building

#### macOS

```bash
# Xcode command line tools
xcode-select --install

# Dependencies (Qt 6 + TBB)
brew install qt tbb

# Put the Homebrew Qt on PATH (works on both Apple Silicon and Intel)
export PATH="$(brew --prefix qt)/bin:$PATH"

git clone https://github.com/BrandonLangdon/autoremesher.git
cd autoremesher
qmake CONFIG+=sdk_no_version_check
make -j$(sysctl -n hw.logicalcpu)
```

The app bundle is built at `autoremesher.app`. Run it with `open ./autoremesher.app`.

> Qt 5 also works: `brew install qt@5` and point `PATH` at `$(brew --prefix qt@5)/bin` instead.

#### Linux (Ubuntu/Debian)

```bash
# Qt 5 and build tools
sudo apt install build-essential qt5-qmake qtbase5-dev qttools5-dev-tools libqt5svg5-dev libqt5multimedia5-dev

# TBB, zlib and OpenGL
sudo apt install libtbb-dev zlib1g-dev libgl1-mesa-dev

# Clone and build
git clone https://github.com/BrandonLangdon/autoremesher.git
cd autoremesher
qmake
make -j$(nproc)
```

> **Fedora:** `sudo dnf install gcc-c++ qt5-qtbase-devel qt5-qttools-devel tbb-devel zlib-devel mesa-libGL-devel`
>
> For a Qt 6 build, install the Qt 6 base/tools packages and run `qmake6` (or the Qt 6 `qmake`) instead of `qmake`.

#### Windows (Visual Studio 2022)

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with **Desktop development with C++** workload.
2. Install [CMake](https://cmake.org/download/) (required to build TBB from source).
3. Install Qt 5.15.2 with the [online installer](https://www.qt.io/download-open-source) — select the `msvc2019_64` archive.
4. Open a **x64 Native Tools Command Prompt for VS 2022** and run:

```cmd
:: Build TBB from the bundled third-party source
cd thirdparty\tbb
cmake -B build2 ^
    -DTBB_BUILD_SHARED=ON ^
    -DTBB_BUILD_STATIC=OFF ^
    -DTBB_BUILD_TBBMALLOC=OFF ^
    -DTBB_BUILD_TBBMALLOC_PROXY=OFF ^
    -DTBB_BUILD_TESTS=OFF
cmake --build build2 --config Release
cd ..\..

:: Build AutoRemesher
qmake -spec win32-msvc
set CL=/MP
nmake -f Makefile.Release
```

The release binary will be at `release\autoremesher.exe`.

## Using the GUI

### Workflow

The GUI is organized around explicit steps so nothing kicks off a long computation unexpectedly:

1. **Open** — load an `.obj`, `.stl`, or `.3mf` file. This only loads and displays the mesh and shows its **source stats** (vertex/triangle counts and bounding-box size). It does **not** remesh.
2. **Run fTetWild** *(optional, only shown when a fTetWild binary is configured)* — rebuilds the surface with fTetWild and adopts its clean output as the working mesh. Useful for large or messy meshes; see [Using fTetWild](#using-ftetwild-optional).
3. **Remesh to Quads** — runs the quad-remeshing pipeline on the current working mesh using the parameters below.
4. **Save** — write the resulting quad mesh to a Wavefront `.obj`.

While a step runs, a captioned spinner over the viewport shows the current stage (e.g. *"Island 3: isotropic remeshing… 42%"*). The **Source / Isotropic / Param / Remeshed** buttons switch the viewport between the input, the intermediate isotropic mesh, the parameterization, and the final quad result.

### Viewport controls

| Action | Gesture |
| --- | --- |
| **Orbit / rotate** | Left-drag (no modifier) |
| **Pan** | **Shift** + left-drag (or middle-drag with a mouse) |
| **Zoom** | Two-finger scroll / mouse wheel |

### Parameters

These control the quad remesh (the **Remesh to Quads** step). The same values are available on the command line.

| Parameter | Range (default) | What it does |
| --- | --- | --- |
| **Target Quads** | 1,000 – 1,000,000 (**50,000**) | Approximate number of quads in the output — the main density control. Internally targets roughly twice this many triangles before quad extraction. |
| **Edge Scaling** | 1.0 – 4.0 (**1.0**) | Multiplies the target edge length. Larger values produce bigger quads / lower-poly output on top of Target Quads; leave at 1.0 for normal density, raise it for deliberately low-poly results. |
| **Adaptivity** | 0.0 – 1.0 (**1.0**) | How strongly quad density follows surface curvature. `0` = uniform quads everywhere; `1` = smaller quads in high-curvature areas (creases, detail) and larger quads on flat regions. Lower it for more even topology. |
| **Sharp Edge** | 30° – 180° (**90°**) | Dihedral-angle threshold for feature detection. Edges that bend more sharply than this are preserved as hard feature edges that the quad flow aligns to. **Lower** it to keep more edges as hard features (good for hard-surface models); **raise** it so only very sharp creases are kept (good for organic shapes). |
| **Smooth Normal** | 0° – 180° (**0°**) | Surface smoothing during remeshing. `0` = faceted (face normals used as-is). Larger values blend vertex normals across edges up to that angle for a smoother remeshed surface — helpful for low-poly inputs that should read as smooth. |

## Using fTetWild (optional)

The built-in isotropic remesher can be slow or unstable on very large, non-manifold, or "dirty" meshes (typical of 3D-print STL/3MF files). For those, AutoRemesher can hand the surface to **[fTetWild](https://github.com/wildmeshing/fTetWild)**, which produces a clean, watertight, uniform-resolution manifold surface that quad remeshing consumes directly. It is **opt-in** — small, clean meshes are faster through the built-in path.

### 1. Build fTetWild

fTetWild is a separate project; build it once and keep the binary. It needs **GMP** and, on current systems, an **older CMake**:

```bash
brew install gmp          # or the platform equivalent
git clone https://github.com/wildmeshing/fTetWild.git
cd fTetWild
# CMake >= 3.30 removed FetchContent_Populate, which fTetWild's bundled
# (2020-era) libigl relies on. Build with CMake 3.29.x.
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces `build/FloatTetwild_bin`.

### 2. Point AutoRemesher at it

- **GUI:** open **File ▸ Preferences** (macOS: ⌘,) and set **fTetWild binary** to the `FloatTetwild_bin` path. The same dialog exposes fTetWild parameters — **envelope size** (`-e`, surface fidelity), **edge length** (`-l`, resolution, relative to the bounding-box diagonal), and **coarsen** (fewer triangles). The path is remembered between sessions.
- **CLI / advanced:** set the `AUTOREMESHER_FTETWILD` environment variable to the binary path.

### 3. Use it

In the GUI, after **Open**, click **Run fTetWild**, then **Remesh to Quads**. On the command line, add `--use-ftetwild` (with `AUTOREMESHER_FTETWILD` set). If fTetWild is unavailable or fails, AutoRemesher falls back to the built-in remesher.

## Command-line (headless)

AutoRemesher has a CLI mode for headless processing. It accepts the same `.obj`, `.stl`, and `.3mf` inputs and writes a Wavefront `.obj`:

```bash
./autoremesher.app/Contents/MacOS/autoremesher \
    --input model.stl \
    --output remeshed.obj \
    --report remeshed_report.txt \
    --target-quads 50000 \
    --edge-scaling 1.0 \
    --sharp-edge 90.0 \
    --smooth-normal 0.0 \
    --adaptivity 1.0
```

Add `--use-ftetwild` to run the fTetWild stage (requires `AUTOREMESHER_FTETWILD`). On Linux/Windows the executable is `./autoremesher` / `autoremesher.exe`. Try it with one of the [common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models).

## Quick Start (prebuilt releases)

> Prebuilt binaries are published by the upstream project and do not include this fork's changes; build from source (above) to use them.

#### Windows

Download `autoremesher-<version>-win32-x86_64.zip` from [releases](https://github.com/huxingyi/autoremesher/releases), extract it and run `autoremesher.exe`.

#### macOS

Download `autoremesher-<version>.dmg` from [releases](https://github.com/huxingyi/autoremesher/releases).

*For the first time, Apple will reject to run and popup something like "can't be opened because its integrity cannot be verified". Go to System Preferences > Security & Privacy > General and under "Allow apps downloaded from" click the button to allow it.*

#### Linux

Download `autoremesher-<version>.AppImage` from [releases](https://github.com/huxingyi/autoremesher/releases).

```
$ chmod a+x ./autoremesher-<version>.AppImage
$ ./autoremesher-<version>.AppImage
```

### Links

- [Check out open-source auto-retopology tool AutoRemesher](http://www.cgchannel.com/2020/08/check-out-open-source-auto-retopology-tool-autoremesher/) **cgchannel.com**
- [A New Open-Source Auto-Retopology Tool](https://80.lv/articles/a-new-open-source-auto-retopology-tool/) **80.lv**
- [[Non-Blender] Autoremesher auto-retopology tool released](https://www.blendernation.com/2020/08/18/non-blender-autoremesher-auto-retopology-tool-released/) **blendernation.com**
- [オープンソースの新しいオートリメッシュツール Auto Remesher](https://cginterest.com/2020/08/20/%e3%82%aa%e3%83%bc%e3%83%97%e3%83%b3%e3%82%bd%e3%83%bc%e3%82%b9%e3%81%ae%e6%96%b0%e3%81%97%e3%81%84%e3%82%aa%e3%83%bc%e3%83%88%e3%83%aa%e3%83%a1%e3%83%83%e3%82%b7%e3%83%a5%e3%83%84%e3%83%bc%e3%83%ab-a/) **cginterest.com**
- [AutoRemesher 1.0.0-alpha - 超高速で高品質のクワッドポリゴン生成！Dust3D開発者によるオープンソースの自動リメッシュツール！](https://3dnchu.com/archives/autoremesher-1-0-0-alpha/) **3dnchu.com**
- [Open Source AutoRemesher released](https://cgpress.org/archives/open-source-remesher.html) **cgpress.org**
- [「autoremesher」多角形を自動でリトポしてれる無料トポロジーツール](https://modelinghappy.com/archives/30339) **modelinghappy.com**
- [Open Source Auto Remesher](https://blender-addons.org/open-source-auto-remesher/) **blender-addons.org**
- [AutoRemesher | Auto-Retopology-Tool](https://www.digitalproduction.com/2020/08/05/autoremesher-auto-retopology-tool/) **digitalproduction.com**
- [Autoremesher open source auto-retopology tool](https://blenderartists.org/t/autoremesher-open-source-auto-retopology-tool/1245131/126) **blenderartists.org**

## License

AutoRemesher is licensed under the MIT License - see the [LICENSE](https://github.com/huxingyi/autoremesher/blob/master/LICENSE) file for details.

## Acknowledgements

See the full [ACKNOWLEDGEMENTS](https://github.com/huxingyi/autoremesher/blob/master/ACKNOWLEDGEMENTS.html) for a list of libraries and resources used in this project.

<!-- Sponsors begin --><!-- Sponsors end -->
