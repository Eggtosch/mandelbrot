C_FLAGS    := -Wall -flto -O3 -g -ggdb
ARCH_FLAGS := -march=native -flax-vector-conversions
SDL_CONFIG := $(shell sdl2-config --cflags --libs)

BIN_DIR := bin

.DEFAULT_GOAL := gui

.PHONY: all
all: normal sse sse_v2 sse_v3 gui

$(BIN_DIR):
	mkdir -p bin/

.PHONY: normal
normal: | $(BIN_DIR)
	gcc $(C_FLAGS) -o bin/mandel src/mandel.c

.PHONY: sse
sse: | $(BIN_DIR)
	gcc $(C_FLAGS) $(ARCH_FLAGS) -o bin/mandel_sse src/mandel_sse.c

.PHONY: sse_v2
sse_v2: | $(BIN_DIR)
	gcc $(C_FLAGS) $(ARCH_FLAGS) -o bin/mandel_sse_v2 src/mandel_sse_v2.c

.PHONY: sse_v3
sse_v3: | $(BIN_DIR)
	gcc $(C_FLAGS) $(ARCH_FLAGS) -o bin/mandel_sse_v3 src/mandel_sse_v3.c

.PHONY: gui
gui: | $(BIN_DIR)
	gcc $(C_FLAGS) $(ARCH_FLAGS) -o bin/mandel_gui src/mandel_gui.c $(SDL_CONFIG)

clean: | $(BIN_DIR)
	rm -f bin/*

