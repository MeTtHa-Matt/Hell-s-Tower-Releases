BUILD_DIR ?= build
BUILD_TYPE ?= Release
APP_VERSION := $(shell git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0-dev")
CFLAGS += -DAPP_VERSION=\"$(APP_VERSION)\"
LDFLAGS += -lcurl

all: configure
	cmake --build $(BUILD_DIR) --parallel
	cmake -E copy_if_different $(BUILD_DIR)/$(TARGET) $(TARGET)

configure:
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ] && \
		! grep -Fqx "CMAKE_HOME_DIRECTORY:INTERNAL=$(CURDIR)" "$(BUILD_DIR)/CMakeCache.txt"; then \
		cmake -E remove "$(BUILD_DIR)/CMakeCache.txt" && \
		cmake -E remove_directory "$(BUILD_DIR)/CMakeFiles"; \
	fi
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_C_FLAGS='$(CFLAGS)' -DCMAKE_EXE_LINKER_FLAGS='$(LDFLAGS)'

ifeq ($(OS),Windows_NT)
TARGET = hells_tower.exe
else
TARGET = hells_tower
endif

ifeq ($(shell uname -s 2>/dev/null),Linux)
LDFLAGS += -lpthread
endif

.PHONY: all configure
