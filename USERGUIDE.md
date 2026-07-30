# User Guide

> Detailed usage reference for the Wavefront Path Tracer. For a project overview, stack and implementation details see `README.md`.

### 1. Prerequisites

#### C++ Dependencies

Refer to the stack table in [README.md](./README.md) for versions. All C++ dependencies must be installed and discoverable by CMake before building.

| Dependency         | macOS (arm64)                         | Linux (NCCA)          |
| ------------------ | ------------------------------------- | --------------------- |
| CMake 3.20+, Ninja | Homebrew                              | system / `$HOME/deps` |
| Intel Embree 4     | Homebrew                              | `$HOME/deps`          |
| Intel oneTBB       | bundled with OpenUSD                  | bundled with OpenUSD  |
| OpenImageIO 2.5    | Homebrew                              | `$HOME/deps`          |
| Intel OIDN 2       | Homebrew                              | `$HOME/deps`          |
| OpenUSD 26.5       | `$HOME/Documents/Projects/USDInstall` | `$HOME/USDInstall`    |

Override the USD path via environment variable:

```bash
export USD_INSTALL_DIR=/path/to/usd
```

#### Python Dependencies

Requires Python 3.12+. All dependencies are declared in `pyproject.toml`. Install with:

```bash
uv sync
```

This installs PySide6 6.7+, pandas 3.0, matplotlib 3.11, seaborn 0.13, OpenImageIO 2.5 and usd-core 26.5.

### 2. Build

```bash
# Full clean build
make rebuild

# Incremental build
make build

# Remove build, venv, generated scenes and figures
make clean-all
```

OpenUSD is always enabled. The Makefile passes `-DENABLE_USD=ON` automatically.

### 3. CLI Usage

```bash
./build/renderer <scene.usda> <output.exr> <camera.usda> [flags]
```

All three positional arguments are optional. If no scene is provided, the built-in Cornell box is rendered and saved to `output/cornellBox.exr`.

#### All Flags

| Flag                  | Default    | Description                                                       |
| --------------------- | ---------- | ----------------------------------------------------------------- |
| `--policy`            | `material` | `none`, `material`, `texture`, `costBenefit`                      |
| `--samples`           | `256`      | Samples per pixel, adaptive sampling may terminate earlier        |
| `--width`             | `600`      | Output image width in pixels                                      |
| `--height`            | `600`      | Output image height in pixels                                     |
| `--max-depth`         | `8`        | Maximum path bounce depth                                         |
| `--denoise`           | off        | Apply OIDN denoiser after rendering                               |
| `--no-adaptive`       | —          | Disable adaptive sampling, all pixels render to full sample count |
| `--cost-rr 0/1`       | `1`        | Enable or disable cost-aware Russian Roulette                     |
| `--ray-sort 0/1`      | `1`        | Enable or disable ray sorting                                     |
| `--env <path>`        | —          | Path to HDRI environment map                                      |
| `--firefly-threshold` | —          | Clamp luminance contributions above this value                    |
| `--progress-interval` | `4`        | Sample interval between progress log lines                        |
| `--memory-stats`      | off        | Print cache coherence statistics after render                     |
| `--quiet`             | off        | Suppress per-sample progress output                               |

#### Examples

```bash
# Built-in Cornell box, default settings
./build/renderer

# Cornell box dragon, 512 samples, denoised
./build/renderer scenes/cornellBoxDragon.usda output/render.exr \
  scenes/cameras/cornellBoxCamera.usda \
  --samples 512 --denoise

# Stress test, costBenefit policy, 4096 samples, 1080x1080
./build/renderer scenes/stressTestDragons.usda output/stress.exr \
  scenes/cameras/cornellBoxCamera.usda \
  --samples 4096 --policy costBenefit --width 1080 --height 1080 --denoise

# With HDRI environment, adaptive sampling disabled
./build/renderer scenes/cornellBoxDragon.usda output/render.exr \
  scenes/cameras/cornellBoxCamera.usda \
  --env scenes/hdri/studio.hdr --samples 256 --no-adaptive
```

### 4. Makefile Targets

All render targets build before rendering unless already up to date.

#### Build

```bash
# Incremental build
make build

# Clean + full build
make rebuild

# Remove build directory
make clean

# Remove build, venv, generated scenes and figures
make clean-all
```

#### Renders

```bash
# Cornell box
make cornell

# Cornell box dragon
make cornell-dragon

# Kitchen set (requires Kitchen_set.usd from Pixar)
make kitchen
```

#### Stress Benchmarks

```bash
make stress-dragons POLICY=none SAMPLES=4096 WIDTH=1080 HEIGHT=1080
make stress-mixed POLICY=costBenefit SAMPLES=4096
```

Each run appends one row to `results/benchmark_results_<samples>.csv`.

#### Makefile Variables

All render targets accept these variables:

| Variable  | Default | Description       |
| --------- | ------- | ----------------- |
| `POLICY`  | `none`  | Scheduling policy |
| `SAMPLES` | `256`   | Samples per pixel |
| `WIDTH`   | `600`   | Image width       |
| `HEIGHT`  | `600`   | Image height      |

#### Other Targets

```bash
# Launch PySide6 GUI
make preview

# Generate all benchmark figures
make reports

# Procedurally generate stress scene USD files
make generate-stress-scenes

# Run GoogleTest suite
make test

# Run ruff and clang-format
make format
```

### 5. GUI

Launch with:

```bash
make preview
```

The GUI has three tabs: **Render**, **Results** and **Compare**.

#### Render Tab

Select a scene and scheduling policy from the dropdowns and click **Render**. A live preview updates every 500ms during rendering. The progress bar and sample counter track completion. Click **Stop** to cancel.

Output EXR files are saved to `output/` as `<scene>_<samples>_<policy>.exr`. Benchmark statistics are automatically parsed from renderer stdout and appended to `results/benchmark_results_<samples>.csv` after each completed render.

#### Results Tab

Click **Refresh** to load all CSVs from `results/`. Results are grouped by sample count into tabs. Each tab shows four charts:

| Chart       | Description                                                                     |
| ----------- | ------------------------------------------------------------------------------- |
| Shade Time  | Mean shading time per policy per scene with % change vs `none` baseline         |
| Pipeline    | Stacked bar breakdown of sort, intersect and shade time with per-stage % change |
| Run Length  | Average consecutive rays hitting the same material per policy                   |
| Homogeneity | Cache line material and texture ID homogeneity per policy and scene             |

#### Compare Tab

Select two rendered EXR files from the dropdowns and click **Compare**. A luminance-difference heatmap is computed using ITU-R BT.709 weights and displayed. Bright areas indicate pixels where the two renders diverged most. Mean absolute difference, maximum difference and per-channel R/G/B statistics are shown below the heatmap.

Click **Refresh** to rescan `output/` for new renders.

### 6. Testing

```bash
make test
```

Runs the full GoogleTest suite. Tests cover ray maths, BVH intersection, material evaluation, scheduling queue behaviour, adaptive sampler convergence, cost tracker EMA accuracy and OpenUSD scene loading.

To run a specific test filter:

```bash
./build/tests --gtest_filter=ShadingQueueTest*
./build/tests --gtest_filter=AdaptiveSamplerTest*
./build/tests --gtest_filter=CostTrackerTest*
```
