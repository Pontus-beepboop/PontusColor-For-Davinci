# Installing PontusColor

You have three options, easiest first. All produce the same `PontusColor.ofx.bundle`, which goes in your system's OFX plugin folder.

OFX plugin folders DaVinci Resolve 21 scans automatically:

- **macOS:** `/Library/OFX/Plugins/`
- **Windows:** `C:\Program Files\Common Files\OFX\Plugins\`

---

## Option A — Prebuilt binaries with GitHub Actions (no toolchain needed)

This is the least fiddly path if you don't want to install developer tools.

1. Create a new GitHub repository and push the contents of this `PontusColor` folder to it (branch `main`).
2. Open the repo's **Actions** tab. The "Build PontusColor" workflow runs automatically; it builds a universal macOS bundle and a Windows bundle.
3. When it finishes (green check), open the run and download the artifact for your OS: **PontusColor-macOS** or **PontusColor-Windows**.
4. Unzip it and copy `PontusColor.ofx.bundle` into the OFX plugin folder for your OS (above).
5. Restart Resolve.

---

## Option B — Build on macOS

**Prerequisites (one time):**

- Xcode command line tools: run `xcode-select --install`
- CMake: `brew install cmake` (or download from cmake.org)

**Build:**

Double‑click `build_mac.command` in Finder. It configures, builds a **universal (Apple Silicon + Intel)** binary, and offers to install it to `/Library/OFX/Plugins/` for you (it will ask for your password and clears the macOS quarantine flag so Resolve loads it).

Prefer the terminal?

```bash
cd PontusColor
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release -j
sudo cp -R build/PontusColor.ofx.bundle /Library/OFX/Plugins/
sudo xattr -dr com.apple.quarantine /Library/OFX/Plugins/PontusColor.ofx.bundle
```

Restart Resolve.

---

## Option C — Build on Windows

**Prerequisites (one time):**

- **Visual Studio 2022** with the "Desktop development with C++" workload.
- **CMake** (cmake.org) — make sure it's added to PATH during install.
- An **OpenCL SDK** (for headers + `OpenCL.lib`). Easiest options:
  - Install the **NVIDIA CUDA Toolkit** (also lets you enable the optional CUDA backend), or
  - Install the AMD/GPUOpen **OCL‑SDK**, or **Intel oneAPI**.

**Build:**

Run `build_windows.bat` (double‑click, or from a terminal). To also build the NVIDIA CUDA backend:

```bat
build_windows.bat cuda
```

Then install by running `install_windows.bat` **as Administrator** (right‑click → Run as administrator), or manually copy `build\PontusColor.ofx.bundle` into `C:\Program Files\Common Files\OFX\Plugins\`.

Restart Resolve.

---

## Using it in Resolve

1. Go to the **Color** page.
2. Open the **Effects** panel (top right) and find **PontusColor** under **OpenFX → Filters** (group "PontusColor").
3. Drag it onto a node in the node graph (or onto the clip).
4. Set **Input Color Space** to match your footage (leave at Rec.709 for standard clips). The defaults already give a filmic look; refine from there.

**Recommended node order:** put PontusColor on its own node. If you grade in a wide‑gamut/log timeline and use Resolve Color Management or a Color Space Transform for output, set PontusColor's **Output Color Space** to *Scene Linear (passthrough)* and let your existing output transform finish the chain. For a standard Rec.709 timeline, leave Output at **Rec.709** and it's display‑ready.

---

## Troubleshooting

**It doesn't appear in the Effects list.** Confirm the bundle is at exactly `/<OFX folder>/PontusColor.ofx.bundle/Contents/<platform>/PontusColor.ofx` and fully restart Resolve (quit, not just close the project). On macOS make sure the quarantine flag was cleared (the build script does this; otherwise run the `xattr -dr` command above).

**macOS "cannot be opened because the developer cannot be verified".** That's the quarantine flag — run `sudo xattr -dr com.apple.quarantine /Library/OFX/Plugins/PontusColor.ofx.bundle`. For distribution to other machines you'd code‑sign the bundle with your Apple Developer ID.

**Windows: CMake can't find OpenCL.** Install one of the OpenCL SDKs listed above, then delete the `build` folder and re‑run. With the CUDA Toolkit installed, OpenCL is found automatically.

**The image looks wrong / too dark or too bright.** The Input Color Space almost certainly doesn't match the footage. Set it to your camera's recording format.
