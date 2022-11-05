#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	int scale;

	if (argc < 2) {
		scale = 1;
	} else {
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
	int yend   = height;

	int print_progress = 0;
	int progress_step  = (yend - ystart) / 100;
	int percent = 0;

	int linesize = xend - xstart;
	unsigned char *line = malloc(linesize);
	fprintf(stderr, "buffer size: %d KB\n", linesize / 1000);

	float ar, ai;
	float br, bi;
	printf("P5\n%d %d\n%d\n", xend - xstart, yend - ystart, 255);

	for (int y = ystart; y < yend; y++) {
		for (int x = xstart; x < xend; x++) {
			ar = 2 * wfactor * (x / (float) width  - 0.5);
			ai = 2 * hfactor * (y / (float) height - 0.5);
			br = 0.0;
			bi = 0.0;

			int i;
			for (i = 0; i < 256 && (br*br) + (bi*bi) < 4; i++) {
				float r0 = br * br - bi * bi;
				float i0 = br * bi + bi * br;
				br = r0;
				bi = i0;
				br += ar;
				bi += ai;
			}
			line[x - xstart] = i;
		}
		fwrite(line, 1, linesize, stdout);
		print_progress++;
		if (print_progress >= progress_step) {
			percent++;
			fprintf(stderr, "\r%d %%", percent);
			print_progress = 0;
		}
	}
	fprintf(stderr, "\r100 %%\n");
	free(line);
	return 0;
}
