# Building AutoRemesher on Windows 11

Step-by-step instructions for building and running AutoRemesher from source on
Windows 11 (x64) with **Qt 6** and **Visual Studio 2022**. Commands are meant to
be copied as-is. Plan on about an hour the first time, mostly downloads and the
first compile.

The same steps work with Qt 5.15 (see [Using Qt 5 instead](#using-qt-5-instead)).
The only difference in the app: with Qt 6 the Windows taskbar icon doesn't show
remesh progress, because Qt 6 removed the API for it. Progress still shows
inside the window.

---

## 1. Install the tools

Open **PowerShell** (Start menu → type `powershell`) and run the commands below.
`winget` is built into Windows 11. Accept any UAC prompts.

```powershell
# Visual Studio 2022 Community with the C++ desktop workload.
# "--includeRecommended" also installs the Windows 11 SDK and CMake.
winget install --id Microsoft.VisualStudio.2022.Community -e --override "--wait --passive --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"

# Git, to clone the repository
winget install --id Git.Git -e

# Python, used only to download Qt with aqtinstall (no Qt account needed)
winget install --id Python.Python.3.12 -e
```

The Visual Studio install is large (several GB) and can take 15–30 minutes.

**Close PowerShell and open a new one** so the new tools are on `PATH`, then
check:

```powershell
git --version
python --version
```

> Already have Visual Studio 2022? Open **Visual Studio Installer → Modify** and
> make sure **Desktop development with C++** is ticked, including
> **C++ CMake tools for Windows** and a **Windows 11 SDK** on the right.

## 2. Install Qt 6

In the same PowerShell window:

```powershell
python -m pip install --upgrade aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt
```

This installs Qt to `C:\Qt\6.8.3\msvc2022_64`. Check it worked:

```powershell
C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe --version
```

It should print `Using Qt version 6.8.3`.

> Prefer the official installer? Use the
> [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) (needs a free
> Qt account), choose **Qt 6.8.x → MSVC 2022 64-bit**, and change the `C:\Qt\...`
> paths below to wherever it installed.

## 3. Get the source

Clone to a **short path** such as `C:\src`. The build writes deeply nested object
files, and long paths can hit Windows' 260-character limit.

```powershell
mkdir C:\src
cd C:\src
git clone https://github.com/BrandonLangdon/autoremesher.git
```

## 4. Open the Visual Studio build prompt

Every command from here on must run in the **x64 Native Tools Command Prompt for
VS 2022**, not in ordinary PowerShell or cmd:

Start menu → type `x64 Native Tools` → open **x64 Native Tools Command Prompt for
VS 2022**.

This is a `cmd` window with the compiler (`cl`), `nmake` and `cmake` on `PATH`.
Check:

```cmd
cl
nmake /?
cmake --version
```

`cl` should print a Microsoft C/C++ compiler banner for **x64**.

## 5. Build TBB (once)

AutoRemesher bundles the TBB threading library source. Build it once:

```cmd
cd C:\src\autoremesher\thirdparty\tbb
cmake -B build2 -DTBB_BUILD_SHARED=ON -DTBB_BUILD_STATIC=OFF -DTBB_BUILD_TBBMALLOC=OFF -DTBB_BUILD_TBBMALLOC_PROXY=OFF -DTBB_BUILD_TESTS=OFF
cmake --build build2 --config Release
cd C:\src\autoremesher
```

Check it produced the DLL:

```cmd
dir thirdparty\tbb\build2\Release\tbb.dll
```

## 6. Build AutoRemesher

Still in the same prompt, in `C:\src\autoremesher`:

```cmd
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
qmake -spec win32-msvc
set CL=/MP
nmake -f Makefile.Release
```

- `set PATH=...` only lasts for this window. Repeat it if you open a new prompt.
- `set CL=/MP` makes the compiler use all CPU cores.
- The first build compiles the whole bundled geometry library and can take a
  while (roughly 5–30 minutes depending on the machine). Warnings are normal. A
  failed build ends with `NMAKE : fatal error`.

When it finishes, the program is at `release\autoremesher.exe`.

## 7. Make it runnable

The exe needs the Qt and TBB DLLs next to it:

```cmd
windeployqt --release --no-translations release\autoremesher.exe
copy thirdparty\tbb\build2\Release\tbb.dll release\
```

Repeat these two commands after every rebuild. They're quick.

## 8. Run it

```cmd
release\autoremesher.exe
```

Or double-click `C:\src\autoremesher\release\autoremesher.exe` in Explorer.

### Quick test of the GUI

1. **Open** an `.obj`, `.stl` or `.3mf` file. The model appears and the source
   stats (vertex/triangle counts, size) show in the left panel.
2. Click **Remesh to Quads**. A spinner shows the current stage and a progress
   bar fills. With Qt 6 the taskbar icon won't show progress; that's expected.
3. Use the **Source / Isotropic / Param / Remeshed** buttons to switch views.
4. **Save** the result as an `.obj` and open it somewhere else to check it.

For a small test model, see
[common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models)
(for example `data/spot.obj` or `data/armadillo.obj`).

### Quick test of the command line

The exe is a GUI program, so `cmd` doesn't wait for it by default. Use
`start /wait` so the prompt waits and the exit code is kept:

```cmd
start /wait "" release\autoremesher.exe --input C:\path\to\model.obj --output C:\src\out.obj --target-quads 5000
echo %ERRORLEVEL%
dir C:\src\out.obj
```

Success is exit code `0` and a new `out.obj`. `1` means the input couldn't be
loaded, and `2` means the remesh produced no geometry. The console log isn't
shown in `cmd`. To capture it, run the same command from **Git Bash**, which
waits and shows output normally:

```bash
./release/autoremesher.exe --input model.obj --output out.obj --target-quads 5000
```

---

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `'nmake' is not recognized` or `'cl' is not recognized` | You're not in the **x64 Native Tools Command Prompt for VS 2022** (step 4). |
| `'qmake' is not recognized` or `'windeployqt' is not recognized` | Run the `set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%` line again in this window. |
| On launch: *"tbb.dll was not found"* | Run `copy thirdparty\tbb\build2\Release\tbb.dll release\`. |
| On launch: *"Qt6Core.dll was not found"* or *"no Qt platform plugin could be initialized"* | Run the `windeployqt` command from step 7. |
| On launch: *"VCRUNTIME140_1.dll / MSVCP140.dll was not found"* (only when copying the exe to another PC) | Install the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe) on that PC. |
| `fatal error C1083: Cannot open ... file` with a very long path | Clone to a shorter path (for example `C:\src`) and rebuild from a clean tree. |
| Build errors about **TBB** headers, or `cannot open file 'tbb.lib'` | Step 5 didn't finish. Re-run it and check `thirdparty\tbb\build2\Release` has `tbb.lib` and `tbb.dll`. |
| Many compile errors inside `thirdparty\` that mention two-phase lookup or `/permissive-` | Qt 6 may turn on MSVC's strict conformance mode, which some older bundled code may not pass. Add `win32: QMAKE_CXXFLAGS += /permissive` near the end of `autoremesher.pro`, clean (row below) and rebuild. Please report the first error so it can be fixed properly. |
| Black or empty 3D viewport (common in VMs or Remote Desktop) | Run `set QT_OPENGL=software` before starting the exe. This uses the software renderer (`opengl32sw.dll`, which `windeployqt` copies). |
| Strange errors after switching Qt versions or pulling big changes | Clean and rebuild: `nmake distclean`, then delete the `release`, `debug`, `obj` and `moc` folders if they're still there, then repeat step 6. |

## Using Qt 5 instead

Qt 5.15.2 also works, and keeps taskbar progress. Install it with:

```powershell
python -m aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 --archives qtbase qttools qtwinextras opengl32sw --outputdir C:\Qt
```

Then use `C:\Qt\5.15.2\msvc2019_64\bin` in place of `C:\Qt\6.8.3\msvc2022_64\bin`
in steps 6 and 7. The MSVC 2019 build of Qt works with Visual Studio 2022.
