BUILD_DIR ?= build
BUILD_TYPE ?= Release

all: configure
	cmake --build $(BUILD_DIR) --parallel
	cmake -E copy_if_different $(BUILD_DIR)/$(TARGET) $(TARGET)

configure:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ] && \
		! grep -Fqx "CMAKE_HOME_DIRECTORY:INTERNAL=$(CURDIR)" "$(BUILD_DIR)/CMakeCache.txt"; then \
		cmake -E remove "$(BUILD_DIR)/CMakeCache.txt" && \
		cmake -E remove_directory "$(BUILD_DIR)/CMakeFiles"; \
	fi
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

ifeq ($(OS),Windows_NT)
TARGET = hells_tower.exe
else
TARGET = hells_tower
endif

.PHONY: all configure
