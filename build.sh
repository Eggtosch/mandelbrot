#!/bin/bash

olddir=$(pwd)
basedir=$(dirname $(readlink -f "$0"))

cd $basedir

mkdir -p bin/

function make_cmd {
	echo $*
	eval $*
}

function make_gui {
	make_cmd gcc -Wall -flto -Ofast -march=native -flax-vector-conversions -g -ggdb -o bin/mandel_gui src/mandel_gui.c $(sdl2-config --cflags --libs)
}

function make_gpu {
	make_cmd gcc -Wall -flto -O3 -march=native -g -ggdb -o bin/mandel_gpu src/mandel_gpu.c $(sdl2-config --cflags --libs) -lopencl-clang
}

if [[ $* == "all" ]]; then
	make_cmd gcc -Wall -flto -O3                                        -g -ggdb -o bin/mandel        src/mandel.c
	make_cmd gcc -Wall -flto -O3 -march=native                          -g -ggdb -o bin/mandel_sse    src/mandel_sse.c
	make_cmd gcc -Wall -flto -O3 -march=native -flax-vector-conversions -g -ggdb -o bin/mandel_sse_v2 src/mandel_sse_v2.c
	make_cmd gcc -Wall -flto -O3 -march=native -flax-vector-conversions -g -ggdb -o bin/mandel_sse_v3 src/mandel_sse_v3.c
	make_gui
	#make_gpu
else
	make_gui
fi

