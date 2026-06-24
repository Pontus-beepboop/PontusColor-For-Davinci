# PontusColor

An OpenFX plugin for **DaVinci Resolve 21** (Color page) that makes it effortless to land a deep, filmic image. It appears in the Color page under **Effects → OpenFX → PontusColor**, works on **macOS and Windows**, and is GPU‑accelerated (Metal on Mac, OpenCL/CUDA on Windows) with a CPU fallback.

Think of it as a faster, more capable Dehancer‑style look engine: drop it on a clip and it already looks like film, then refine with a handful of musical controls.

## What makes it look like film

The headline is **subtractive saturation**. Ordinary saturation pushes colours toward pure RGB primaries and goes "electric" and clippy. PontusColor instead works in an inverted dye‑density domain — it holds the neutral grey component and scales only the pure dye part, so colours get *denser* and shift hue the way film emulsion does, never neon. Layered on top:

- **Filmic tone curve** — a log‑pivot contrast S‑curve with a soft highlight shoulder for creamy, non‑clipping highlights and a gentle toe.
- **Highlight & shadow desaturation** — colour bleeds out of the extremes, the classic film signature.
- **Print emulation** — warm shadows / cooler highlights split‑tone plus print density for rich blacks.
- **Luminance‑preserving white balance** — Temperature and Tint that don't change overall brightness, easy to push warm/cool.

## Works with any source

By default the input is treated as **Rec.709**. An **Input Color Space** dropdown correctly decodes log/wide‑gamut footage into scene‑linear before the look, so the same grade works on:

Rec.709, Linear, Sony **S‑Log3** (S‑Gamut3 & S‑Gamut3.Cine), **RED Log3G10** (REDWideGamutRGB), ARRI **LogC3** & **LogC4**, Panasonic **V‑Log**, Blackmagic **Film Gen5**, DJI **D‑Log**, Canon **Log3**, Fujifilm **F‑Log2**, and **Cineon**.

The gamut matrices are computed from each camera's published primaries (see `tools/gamut.py`) and match Resolve's own Color Space Transform within rounding.

## Controls

Base Look (Neutral, Filmic Subtractive, Bleach Bypass, Warm Print, Cool Modern, Vintage) sets a starting point; every slider layers on top.

| Group | Controls |
|---|---|
| Color Management | Input Color Space, Output Color Space |
| Exposure & Balance | Exposure (stops), Temperature, Tint |
| Tone | Contrast, Contrast Pivot, Highlight Rolloff, Toe / Black Soften |
| Color | Subtractive Saturation, Saturation, Highlight Desat, Shadow Desat |
| Print Emulation | Print Warmth, Print Density |
| Master | Look Intensity (dry/wet mix) |

The **defaults already produce a finished filmic look** on a Rec.709 clip — set the Input Color Space to match your footage and you're most of the way there.

## Install

**Easiest — prebuilt binaries via GitHub Actions:** push this folder to a GitHub repo. The included workflow builds macOS (universal) and Windows automatically; download the bundle from the run's *Artifacts* and drop it in your OFX plugins folder (paths below).

**Build locally:**
- **macOS:** double‑click `build_mac.command` (needs Xcode command line tools + CMake). It offers to install for you.
- **Windows:** run `build_windows.bat` (needs Visual Studio 2022 + CMake + an OpenCL SDK), then `install_windows.bat` as Administrator.

OFX plugin folders Resolve scans:
- macOS: `/Library/OFX/Plugins/`
- Windows: `C:\Program Files\Common Files\OFX\Plugins\`

Restart Resolve, then open the **Color page → Effects → OpenFX → PontusColor** and drag it onto a node.

See **INSTALL.md** for full, step‑by‑step instructions and troubleshooting.

## How it's built

`src/PontusColorKernel.h` is the single source of truth for the look math, compiled by every backend (Metal / OpenCL / CUDA / CPU) through a thin macro layer so the image is identical on every GPU and OS. `src/PontusColorPlugin.cpp` is the OFX host integration (built on the OpenFX C++ Support library, vendored in `openfx/`). The colour math has been unit‑tested for an exact neutral pass‑through and validated end‑to‑end.

Licensed for your use; the bundled OpenFX SDK is under its own BSD‑style license (`openfx/LICENSE`).
