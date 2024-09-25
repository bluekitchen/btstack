# Makefile to build and run tests of CMake project
# TODO: replace list of hard-coded tests with CMake call to run all tests

# Variables
BUILD_DIR = build
TARGET = all  # Default build target

# Default rule
all: configure build

# Step 1: Configure CMake
configure:
	@echo "Configuring project in $(BUILD_DIR)..."
	@mkdir -p $(BUILD_DIR)
	@cd ${BUILD_DIR} && cmake ..

# Step 2: Build the project
build: configure
	@echo "Building project in $(BUILD_DIR)..."
	@cmake --build $(BUILD_DIR) --target $(TARGET)

# Step 3: Run the tests
test: build
	@echo "Running tests..."
	$(BUILD_DIR)/btstack_util_test

# Step 4: Clean the build
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

coverage:
	@echo "No coverage tests in btstack_util"

.PHONY: all configure build clean test coverage
