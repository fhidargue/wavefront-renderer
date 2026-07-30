# Wavefront Path Tracer

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![CMake](https://img.shields.io/badge/CMake-3.8+-blue)
![Embree](https://img.shields.io/badge/Embree-4.x-orange)
![oneTBB](https://img.shields.io/badge/oneTBB-supported-orange)
![OpenUSD](https://img.shields.io/badge/OpenUSD-supported-brightgreen)
![OpenImageDenoise](https://img.shields.io/badge/OpenImageDenoise-2.x-brightgreen)
![PySide6](https://img.shields.io/badge/PySide6-preview%20UI-9cf)
![GoogleTest](https://img.shields.io/badge/GoogleTest-enabled-yellow)

![Material Variants](./output/readme/materialVariants.png)

A CPU wavefront path tracer built as part of the MSc thesis _"Adaptive Scheduling for Coherent Wavefront Rendering"_ (NCCA, Bournemouth University, 2026). The project investigates whether reordering rays by material or texture ID before shading improves CPU cache utilisation and reduces render time.

<p align="center">
  <img src="./output/readme/cornellDragon.gif" width="600"><br>
  <em>Cornell box dragon, 256spp</em>
</p>

### Stack

|                    | Version                                   |
| ------------------ | ----------------------------------------- |
| C++                | 17                                        |
| Python             | 3.12+                                     |
| Build              | CMake 3.20+, Ninja                        |
| BVH / intersection | Intel Embree 4                            |
| Parallelism        | Intel oneTBB (bundled with USD)           |
| Scene format       | OpenUSD 26.5                              |
| Image I/O          | OpenImageIO 2.5                           |
| Denoising          | Intel Open Image Denoise 2                |
| GUI                | PySide6 6.7+                              |
| Plotting           | matplotlib 3.11, seaborn 0.13, pandas 3.0 |
| Testing            | GoogleTest                                |

### The Research Question

In a wavefront path tracer, rays at the same bounce depth are processed in bulk. In the naive case, rays arrive in arbitrary order, consecutive shading operations touch different materials, textures and shader branches, causing cache thrashing. The hypothesis: **sorting rays by material or texture ID before shading should improve cache line homogeneity and reduce total shading time.**

Four scheduling policies were implemented and benchmarked:

| Policy        | Description                                                                               |
| ------------- | ----------------------------------------------------------------------------------------- |
| `none`        | No sorting. Default arrival order, used as baseline                                       |
| `material`    | Sort by material ID using TBB parallel sort                                               |
| `texture`     | Sort by texture ID targeting texture cache coherence                                      |
| `costBenefit` | Sort by material ID, then apply Morton code spatial sub-sort weighted by EMA shading cost |

### Implementation

#### 1. Wavefront Pipeline

The renderer processes rays in a **Structure of Arrays (SoA)** layout for cache friendly access. Each bounce iterates:

```
Generate camera rays (RayQueue)
  → Embree BVH traversal (parallel, per-ray)
    → Populate ShadingQueue (hit point, normal, material ID, texture ID, throughput)
      → [Optional] Sort ShadingQueue by scheduling policy
        → Shade all hits and scatter new rays into next RayQueue
          → Repeat until max depth or Russian Roulette termination
```

#### 2. Embree BVH Integration

Scene geometry is handed off to Intel Embree 4 via a custom wrapper that handles triangle soup construction, multi-mesh scene registration and ray packet traversal. All intersection queries run through Embree's BVH, with hit results barycentric coordinates, triangle index and geometry ID. Mapped back to the renderer's `HitRecord` for material and texture lookup.

#### 3. OpenUSD Scene Loading

Scenes are described in `.usda` files and loaded via a full OpenUSD pipeline. The loader resolves mesh geometry, world transforms, material bindings, UV primvars, texture assets, camera parameters and emissive light sources. Supported primitives include meshes, spheres, cylinders and cubes. Material properties as diffuse colour, roughness, metallic, IOR and custom renderer extensions are parsed from shader inputs. HDRI environment maps are importance-sampled using a 2D luminance distribution built at load time.

#### 4. Multiple Importance Sampling and Next Event Estimation

At each bounce, direct lighting is estimated via **Next Event Estimation (NEE)**. A shadow ray is casted explicitly toward a sampled light source, bypassing the need for a path to randomly hit it. The direct contribution is then combined with the BSDF-sampled contribution using **Multiple Importance Sampling (MIS)** and the power heuristic, weighting each strategy by its PDF to minimise variance. This significantly reduces noise on scenes with small or bright area lights without requiring additional path samples.

#### 5. Scheduling Policies (`ShadingQueue`)

All policies produce a `sortedIndices` array that the shading loop iterates. No data is moved, only the traversal order changes.

- **None**: rays shaded in arrival order, used as baseline.
- **MaterialAware**: `tbb::parallel_sort` by material ID. Consecutive shading calls hit the same shader branch and material parameters, keeping them warm in cache.
- **TextureAware**: sort by texture ID, targeting texture sampler cache coherence.
- **CostBenefitAware**: sort by material ID first, then apply a **Morton code spatial sub-sort** within each material group to preserve BVH locality for the next bounce. Sort weight is modulated by a per-material EMA shading cost.

#### 6. Adaptive Sampling (`AdaptiveSampler`)

Per-pixel luminance convergence is tracked using **Welford's online algorithm**, a numerically stable single-pass method for running mean and variance. A pixel stops receiving samples when its standard error of the mean drops below 5% of the running mean. This concentrates work on noisy regions without a second pass.

#### 7. Cost-Aware Russian Roulette (`CostTracker`)

An **Exponential Moving Average** (α = 0.05) tracks shading time per material ID in nanoseconds. Russian Roulette termination probability is scaled by `relativeCost(materialID)`: the ratio of this material's average cost to the global average. Expensive materials are terminated earlier, reducing average shading cost per bounce.

#### 8. Benchmark Pipeline

A full research toolchain automates data collection and visualisation. The renderer writes structured statistics to stdout after each run. `parse_results.py` extracts metrics via regex into per-sample-count CSVs. `plot_results.py` produces figures using pandas and seaborn: shade time comparisons, pipeline breakdowns, run length distributions and cache homogeneity charts. All figures are surfaced in a PySide6 GUI with per-sample-bucket tabs and a luminance-difference heatmap compare view.

### Scenes

| Cornell Box                                                   | Cornell Box Dragon                                                   |
| ------------------------------------------------------------- | -------------------------------------------------------------------- |
| ![Cornell Box](./output/readme/cornellBox_512_none.png)       | ![Cornell Box Dragon](./output/readme/cornellBoxDragon_512_none.png) |
| **Stress Test Mixed**                                         | **Stress Test Dragons**                                              |
| ![Stress Mixed](./output/readme/stressTestMixed_512_none.png) | ![Stress Dragons](./output/readme/stressTestDragons_512_none.png)    |

**Cornell Box**: Classic rendering reference scene used to validate physically-based light transport. Features diffuse colors and OpenUSD scene loading. Used to verify correctness of the BVH, materials and path tracing implementation.

**Cornell Box Dragon**: Cornell box variant featuring a Stanford dragon with a spatial checker texture and a glass teapot on a metallic pedestal. Demonstrates mixed material handling with diffuse, glass, metallic and procedural texture shading within a single scene.

**Stress Test Mixed**: A grid of ~90 objects mixing teapots and dragons, each assigned one of 30+ procedurally generated materials spanning diffuse, plastic, metal and glass types with texture maps ranging from 256px to 4096px. The high material and texture diversity makes this the most demanding benchmark scene for measuring scheduling coherence gains.

**Stress Test Dragons**: A dense grid of xyzrgb dragons totalling ~17.9M triangles, each assigned varied materials. The extreme geometric complexity makes this the heaviest benchmark scene, stress-testing BVH traversal performance and exposing how scheduling policies behave under high triangle counts.

### Benchmarks

| Result Graphs                                                           | Heatmap Comparison                                                |
| ----------------------------------------------------------------------- | ----------------------------------------------------------------- |
| <img src="./output/readme/results-graph.png" height="600" width="500"/> | <img src="./output/readme/heatmap.png" height="600" width="500"/> |

### Build

```bash
make rebuild
```

Requires Embree 4, oneTBB, OpenUSD and OIDN installed and discoverable by CMake.

### Render

```bash
./build/renderer scenes/cornellBoxDragon.usda output/render.exr \
  --samples 256 --policy costBenefit --denoise
```

| Flag                   | Default    | Description                                      |
| ---------------------- | ---------- | ------------------------------------------------ |
| `--policy`             | `material` | `none`, `material`, `texture`, `costBenefit`     |
| `--samples`            | `256`      | Samples per pixel                                |
| `--width` / `--height` | `600`      | Output resolution                                |
| `--max-depth`          | `8`        | Maximum path bounce depth                        |
| `--denoise`            | off        | Apply OIDN denoiser to final image               |
| `--no-adaptive`        | —          | Disable adaptive sampling                        |
| `--cost-rr`            | `1`        | Cost-aware Russian Roulette (`0` to disable)     |
| `--ray-sort`           | `1`        | Ray sorting (`0` to disable)                     |
| `--env`                | —          | Path to environment map (HDR)                    |
| `--firefly-threshold`  | —          | Clamp firefly contributions above this luminance |
| `--progress-interval`  | `4`        | Sample interval between progress updates         |
| `--memory-stats`       | off        | Print memory coherence statistics after render   |
| `--quiet`              | off        | Suppress per-sample progress output              |

## Test

```bash
make test
```

_For scene setup, GUI usage and benchmark pipeline details see `USERGUIDE.md`_

---

_Status: active development, thesis due August 2026._
