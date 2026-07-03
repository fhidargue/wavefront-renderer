BUILD_DIR = build
WIDTH ?= 600
HEIGHT ?= 600
KITCHEN_SET_PATH=$(HOME)/Downloads/Kitchen_set

export PXR_AR_DEFAULT_SEARCH_PATH := $(KITCHEN_SET_PATH)

.PHONY: all build clean rebuild test cornell kitchen preview preview-kitchen format

all: build

build:
	@cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_USD=ON \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -Wno-dev 2>/dev/null || true
	@ninja -C $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build

test: build
	@./$(BUILD_DIR)/tests

cornell: build
	@./$(BUILD_DIR)/renderer scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda \
		--width $(WIDTH) --height $(HEIGHT)

kitchen: build
	@PXR_AR_DEFAULT_SEARCH_PATH=$(KITCHEN_SET_PATH) ./$(BUILD_DIR)/renderer scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda \
		--width $(WIDTH) --height $(HEIGHT)

preview: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) uv run python3 -m gui.main scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda 

preview-kitchen: build
	@WIDTH=$(WIDTH) HEIGHT=$(HEIGHT) PXR_AR_DEFAULT_SEARCH_PATH=$(KITCHEN_SET_PATH) uv run python3 -m gui.main scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda

format:
	@find . -name "*.cpp" -o -name "*.h" | grep -v "/build/" | xargs clang-format -i
	@ruff format .