SHELL := /usr/bin/bash

CMAKE ?= /usr/bin/cmake
BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= $(HOME)/.local
JOBS ?=

.PHONY: all configure build install uninstall clean run reload help

all: build

help:
	@printf '%s\n' \
		'make configure           Configure the project into $(BUILD_DIR)' \
		'make build               Build ward' \
			'make install             Install ward into $(PREFIX)/bin' \
		'make uninstall           Remove files recorded by the last install' \
		'make clean               Clean the build directory outputs' \
		'make run                 Run the built binary from $(BUILD_DIR)' \
		'make reload              Ask a running ward instance to reload config' \
		'' \
		'Overrides:' \
		'  BUILD_DIR=/path/to/build' \
		'  BUILD_TYPE=Debug|Release|RelWithDebInfo|MinSizeRel' \
			'  PREFIX=$$HOME/.local or /usr/local' \
		'  JOBS=-j8' \
		'' \
		'Dry run:' \
		'  make -n install PREFIX=$$HOME/.local'

configure:
	"$(CMAKE)" -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)"

build: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" $(JOBS)

install: build
	"$(CMAKE)" --install "$(BUILD_DIR)" --prefix "$(PREFIX)"

uninstall:
	@if [[ ! -f "$(BUILD_DIR)/install_manifest.txt" ]]; then \
		printf '%s\n' 'No install manifest found in $(BUILD_DIR). Run make install first.' >&2; \
		exit 1; \
	fi
	while IFS= read -r file_path || [[ -n "$$file_path" ]]; do \
		if [[ -n "$$file_path" ]]; then \
			rm -f "$$file_path"; \
		fi; \
	done < "$(BUILD_DIR)/install_manifest.txt"

clean: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --target clean

run: build
	"./$(BUILD_DIR)/ward"

reload: build
	"./$(BUILD_DIR)/ward" --reload
