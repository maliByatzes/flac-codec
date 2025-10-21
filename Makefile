.ONESHELL:

ifeq ($(OS),Windows_NT)
	EXE := .exe
	SEP := \\
	EXEC_PREFIX :=
else
	EXE :=
	SEP := /
	EXEC_PREFIX := ./
endif

setup:
	cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build build --config Debug -- -j2

run: build
	@echo Running main executable...
	"${EXEC_PREFIX}build$(SEP)src$(SEP)flac_codec$(EXE)"
