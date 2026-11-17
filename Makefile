BUILD_DIR ?= build
BUILD_TYPE ?= Release

all: configure
	cmake --build $(BUILD_DIR) --parallel
	cmake -E copy_if_different $(BUILD_DIR)/$(TARGET) $(TARGET)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

ifeq ($(OS),Windows_NT)
TARGET = hells_tower.exe
else
TARGET = hells_tower
endif

.PHONY: all configure
