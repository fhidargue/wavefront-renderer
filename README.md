# Wavefront Renderer

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![CMake](https://img.shields.io/badge/CMake-3.8+-blue)
![Embree](https://img.shields.io/badge/Embree-4.x-orange)
![oneTBB](https://img.shields.io/badge/oneTBB-supported-orange)
![OpenUSD](https://img.shields.io/badge/OpenUSD-supported-brightgreen)
![OpenImageDenoise](https://img.shields.io/badge/OpenImageDenoise-2.x-brightgreen)
![PySide6](https://img.shields.io/badge/PySide6-preview%20UI-9cf)
![GoogleTest](https://img.shields.io/badge/GoogleTest-enabled-yellow)

A CPU-based wavefront path tracer investigating adaptive scheduling for coherent shading, built for the MSc thesis _"Adaptive Scheduling for Coherent Wavefront Rendering"_ (NCCA, Bournemouth University).

<p align="center">
  <img src="./output/video/cornellDragon.gif" width="600"><br>
  <em>Cornell box dragon, 256spp</em>
</p>

## Stack

- **Core**: C++17, CMake + Ninja
- **Acceleration**: Embree 4 (BVH traversal)
- **Parallelism**: oneTBB
- **Scene description**: OpenUSD (pxr)
- **Denoising**: Intel Open Image Denoise
- **Preview UI**: PySide6 (live progress + OpenGL display)
- **Testing**: GoogleTest

## Features

- SoA (structure-of-arrays) wavefront pipeline: `RayQueue` → `ShadingQueue` → next bounce
- Material/texture-aware shading schedulers (`None`, `MaterialAware`, `MaterialAwareParallel`, `TextureAware`)
- Material-cost-aware Russian Roulette (EMA-based cost tracking)
- Morton/octant-based ray sorting for traversal coherence _(implemented, benchmarked as net-neutral on cache-resident scenes — see thesis results)_
- Per-pixel adaptive sampling (Welford-based convergence tracking)
- Cross-platform: macOS (arm64) and Linux (NCCA lab machines)

## Build

```bash
make rebuild
```

## Render

```bash
./build/renderer scenes/cornellBoxDragon.usda output/render.exr --samples 256 --max-depth 8
```

Useful flags: `--policy [none|material|material-parallel|texture]`, `--no-ray-sort`, `--no-adaptive`, `--no-cost-rr`, `--denoise`.

## Test

```bash
./build/tests
```

---

_Status: active development, thesis due August 2026._
