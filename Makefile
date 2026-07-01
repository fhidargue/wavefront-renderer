BUILD_DIR = build

.PHONY: all build clean rebuild test render cornell kitchen preview

all: build

build:
	cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=$(HOME)/Documents/Projects/vcpkg/scripts/buildsystems/vcpkg.cmake \
		-DENABLE_USD=ON \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) --parallel

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build

test: build
	@./$(BUILD_DIR)/tests

cornell: build
	@./$(BUILD_DIR)/renderer scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda

kitchen: build
	@./$(BUILD_DIR)/renderer scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda

preview: build
	@uv run python3 -m gui.main scenes/cornellBox.usda output/cornellBox.exr \
		scenes/cameras/cornellBoxCamera.usda

preview-kitchen: build
	@uv run python3 -m gui.main scenes/kitchenSet.usda \
		output/kitchen.exr scenes/cameras/kitchenSetCamera.usda