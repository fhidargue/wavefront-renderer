BUILD_DIR = build

.PHONY: all build clean rebuild test render cornell

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

render: build
	@./$(BUILD_DIR)/renderer scenes/cornellBox.usda output/cornellBox.exr

cornell: build
	@./$(BUILD_DIR)/renderer output/cornellBoxHardcoded.exr

preview: build
	@uv run python3 -m gui.main scenes/cornellBox.usda output/cornellBox.exr