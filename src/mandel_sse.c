#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVX_WIDTH 8

typedef float avx_reg __attribute__((vector_size(AVX_WIDTH * sizeof(float))));
typedef int int_vec __attribute__((vector_size(AVX_WIDTH * sizeof(int))));

int main(int argc, char **argv) {
	int scale = 1;

	if (argc >= 2) {
		scale = atoi(argv[1]);
	}
	if (scale < 1) {
		fprintf(stderr, "Scale must be greater than 0!");
		return 1;
	}

	int width  = scale * 1600;
	int height = scale *  900;
	float accuracy = (float) scale * 1000.0;
	float wfactor  = width  / accuracy;
	float hfactor  = height / accuracy;

	int xstart = 0;
	int xend   = width;
	int ystart = 0;
	int yend   = height / 2;

	int print_progress = 0;
	int progress_step  = (yend - ystart) / 100;
	int percent = 0;

	int linesize = xend - xstart;
	unsigned char *line = malloc(linesize * (yend + 1));
	fprintf(stderr, "buffer size: %d MB\n", linesize * yend / 1000000);

	printf("P5\n%d %d\n%d\n", xend - xstart, yend * 2 - ystart, 255);

	for (int y = ystart; y <= yend; y++) {
		for (int x = xstart; x < xend; x += AVX_WIDTH) {
			avx_reg xv = {x,x,x,x,x,x,x,x};
			avx_reg yv = {y,y,y,y,y,y,y,y};
			avx_reg adds = {0,1,2,3,4,5,6,7};
			avx_reg ar = 2 * wfactor * ((xv + adds) / (float) width  - 0.5);
			avx_reg ai = 2 * hfactor * ( yv         / (float) height - 0.5);
			avx_reg br = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
			avx_reg bi = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

			int num_finished = 0;
			int_vec results = {0, 0, 0, 0, 0, 0, 0, 0};
			for (int i = 0; i < 256; i++) {
				avx_reg square_sums = br * br + bi * bi;
				int_vec is_finished = square_sums >= 4.0;
				for (int n = 0; n < AVX_WIDTH; n++) {
					if (is_finished[n] && results[n] == 0) {
						results[n] = i;
						num_finished++;
					}
				}
				if (num_finished == AVX_WIDTH) {
					break;
				}

				avx_reg r0 = br * br - bi * bi;
				avx_reg i0 = br * bi + bi * br;
				br = r0;
				bi = i0;
				br += ar;
				bi += ai;
			}
			for (int n = 0; n < AVX_WIDTH; n++) {
				line[y * width + (x - xstart + n)] = results[n];
			}
		}
		print_progress++;
		if (print_progress >= progress_step) {
			percent++;
			fprintf(stderr, "\r%d %%", percent);
			print_progress = 0;
		}
	}

	fwrite(line, 1, linesize * (yend + 1), stdout);
	for (int n = yend - 1; n > 0; n--) {
		fwrite(line + n * width, 1, linesize, stdout);
	}

	fprintf(stderr, "\r100 %%\n");
	free(line);
	return 0;
}
