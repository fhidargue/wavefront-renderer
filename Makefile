BUILD_DIR = build
VENV_DIR = .venv
WIDTH ?= 600
HEIGHT ?= 600
COST_RR ?= 1
ENV ?=
POLICY ?= none
SAMPLES ?= 256

KITCHEN_SET_PATH = $(HOME)/Downloads/Kitchen_set
export PXR_AR_DEFAULT_SEARCH_PATH := $(KITCHEN_SET_PATH)

# Golden
GOLDEN_SAMPLES ?= 500000
GOLDEN_MAX_DEPTH ?= 64
GOLDEN_PROGRESS_INTERVAL ?= 5000

# Flags
ifeq ($(COST_RR),0)
COST_RR_FLAG = --cost-rr 0
else
COST_RR_FLAG = --cost-rr 1
endif

ifeq ($(RAY_SORT),0)
RAY_SORT_FLAG = --ray-sort 0
else
RAY_SORT_FLAG = --ray-sort 1
endif

ifneq ($(ENV),)
ENV_FLAG = --env $(ENV)
endif

POLICY_FLAG = --policy $(POLICY)
SAMPLES_FLAG = --samples $(SAMPLES)

COMMON_FLAGS = --quiet --width $(WIDTH) --height $(HEIGHT) --denoise \
               $(COST_RR_FLAG) $(RAY_SORT_FLAG) $(ENV_FLAG)

CAMERA = scenes/cameras/cornellBoxCamera.usda
KITCHEN_CAM = scenes/cameras/kitchenSetCamera.usda
RENDERER = ./$(BUILD_DIR)/renderer

# Phony
.PHONY: all build clean clean-build clean-scripts clean-all rebuild test \
        cornell cornell-dragon kitchen golden-render preview \
        format generate-stress-scenes stress-dragons stress-mixed reports

# Build
all: build

build:
	@cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_USD=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev 2>/dev/null || true
	@ninja -C $(BUILD_DIR)

rebuild: clean build

# Clean
clean:
	@rm -rf $(BUILD_DIR)

clean-build:
	@rm -rf $(BUILD_DIR) $(VENV_DIR)

clean-scripts:
	@rm -rf scenes/textures/generated scenes/stress*.usda results/figures

clean-all: clean-build clean-scripts

# Test
test: build
	@./$(BUILD_DIR)/tests

# Renders
cornell: build
	@$(RENDERER) scenes/cornellBox.usda output/cornellBox.exr \
		$(CAMERA) $(COMMON_FLAGS)

cornell-dragon: build
	@$(RENDERER) scenes/cornellBoxDragon.usda output/cornellBoxDragon.exr \
		$(CAMERA) --memory-stats $(COMMON_FLAGS)

kitchen: build
	@PXR_AR_DEFAULT_SEARCH_PATH=$(KITCHEN_SET_PATH) \
		$(RENDERER) scenes/kitchenSet.usda output/kitchen.exr \
		$(KITCHEN_CAM) $(COMMON_FLAGS)

golden-render: build
	@echo "Starting golden render: $(GOLDEN_SAMPLES) samples, depth $(GOLDEN_MAX_DEPTH)"
	@$(RENDERER) scenes/cornellBoxDragon.usda output/cornellBoxDragon_golden.exr \
		$(CAMERA) --quiet --width $(WIDTH) --height $(HEIGHT) \
		--samples $(GOLDEN_SAMPLES) --max-depth $(GOLDEN_MAX_DEPTH) \
		--policy none --cost-rr 0 --ray-sort 0 \
		--progress-interval $(GOLDEN_PROGRESS_INTERVAL)

# Stress
define run_stress
	@set -o pipefail; $(RENDERER) scenes/$(1).usda output/$(1).exr \
		$(CAMERA) --memory-stats $(COMMON_FLAGS) \
		$(POLICY_FLAG) $(SAMPLES_FLAG) \
		2>&1 | tee /tmp/render_output.txt
	uv run python scripts/results/parse_results.py /tmp/render_output.txt $(1) $(POLICY)
endef

stress-dragons: build
	$(call run_stress,stressTestDragons)

stress-mixed: build
	$(call run_stress,stressTestMixed)

# Preview
preview: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main \
		scenes/cornellBox.usda output/cornellBox.exr $(CAMERA) \
		--quiet --denoise --memory-stats $(COST_RR_FLAG) $(RAY_SORT_FLAG) $(ENV_FLAG)

# Reports
reports:
	@uv run python scripts/results/plot_results.py

# Format
format:
	@find . -name "*.cpp" -o -name "*.h" | grep -v "/build/" | xargs clang-format -i
	@uv run ruff format .
	@uv run ruff check --fix .

# Scripts
generate-stress-scenes:
	@rm -rf scenes/textures/generated
	@uv run scripts/generate_stress_scenes.py