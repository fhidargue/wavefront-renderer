BUILD_DIR = build
VENV_DIR = .venv
WIDTH ?= 600
HEIGHT ?= 600
COST_RR ?= 1
ENV ?=
KITCHEN_SET_PATH=$(HOME)/Downloads/Kitchen_set

# Golden render settings
GOLDEN_SAMPLES ?= 500000
GOLDEN_MAX_DEPTH ?= 64
GOLDEN_PROGRESS_INTERVAL ?= 5000

export PXR_AR_DEFAULT_SEARCH_PATH := $(KITCHEN_SET_PATH)

ifeq ($(COST_RR),0)
COST_RR_FLAG = --no-cost-rr
else
COST_RR_FLAG =
endif

ifneq ($(ENV),)
ENV_FLAG = --env $(ENV)
else
ENV_FLAG =
endif

.PHONY: all build clean clean-all clean-scripts rebuild test cornell kitchen cornell-dragon golden-render preview preview-cornell-dragon preview-kitchen format generate-stress-scenes stress-dragons stress-mixed preview-stress-dragons preview-stress-mixed

all: build

build:
	@cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_USD=ON \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev 2>/dev/null || true
	@ninja -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)

clean-all:
	@rm -rf $(BUILD_DIR) $(VENV_DIR)

clean-scripts:
	@rm -rf scenes/textures/generated scenes/stress*.usda

rebuild: clean build

test: build
	@./$(BUILD_DIR)/tests

cornell: build
	@./$(BUILD_DIR)/renderer scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) --denoise $(COST_RR_FLAG) $(ENV_FLAG)

cornell-dragon: build
	@./$(BUILD_DIR)/renderer scenes/cornellBoxDragon.usda output/cornellBoxDragon.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) --denoise $(COST_RR_FLAG) $(ENV_FLAG)

kitchen: build
	@PXR_AR_DEFAULT_SEARCH_PATH=$(KITCHEN_SET_PATH) ./$(BUILD_DIR)/renderer scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) --denoise $(COST_RR_FLAG) $(ENV_FLAG)

preview: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda --quiet --denoise $(ENV_FLAG)

preview-cornell-dragon: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main scenes/cornellBoxDragon.usda output/cornellBoxDragon.exr \
		scenes/cameras/cornellBoxCamera.usda --quiet --denoise $(ENV_FLAG)

preview-kitchen: build
	@WIDTH=900 HEIGHT=700 PXR_AR_DEFAULT_SEARCH_PATH=$(KITCHEN_SET_PATH) uv run python3 -m gui.main scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda --quiet --denoise $(ENV_FLAG)

format:
	@find . -name "*.cpp" -o -name "*.h" | grep -v "/build/" | xargs clang-format -i
	@ruff format .

golden-render: build
	@echo "Starting golden render: $(GOLDEN_SAMPLES) samples, depth $(GOLDEN_MAX_DEPTH), no scheduling, no cost-aware RR"
	@./$(BUILD_DIR)/renderer scenes/cornellBoxDragon.usda output/cornellBoxDragon_golden.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) \
		--samples $(GOLDEN_SAMPLES) --max-depth $(GOLDEN_MAX_DEPTH) \
		--policy none --no-cost-rr --no-ray-sort \
		--progress-interval $(GOLDEN_PROGRESS_INTERVAL)

generate-stress-scenes:
	@rm -rf scenes/textures/generated 
	@uv run scripts/generate_stress_scenes.py

stress-dragons: build
	@./$(BUILD_DIR)/renderer scenes/stressTestDragons.usda output/stressTestDragons.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) --denoise $(COST_RR_FLAG) $(ENV_FLAG)

stress-mixed: build
	@./$(BUILD_DIR)/renderer scenes/stressTestMixed.usda output/stressTestMixed.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--quiet --width $(WIDTH) --height $(HEIGHT) --denoise $(COST_RR_FLAG) $(ENV_FLAG)

preview-stress-dragons: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main scenes/stressTestDragons.usda output/stressTestDragons.exr \
		scenes/cameras/cornellBoxCamera.usda --quiet --denoise $(ENV_FLAG)

preview-stress-mixed: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main scenes/stressTestMixed.usda output/stressTestMixed.exr \
		scenes/cameras/cornellBoxCamera.usda --quiet --denoise $(ENV_FLAG)
