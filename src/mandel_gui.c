#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#include <SDL.h>


typedef double   mandel_float;
typedef long int mandel_int;
#define AVX_WIDTH (32 / sizeof(mandel_float))
typedef mandel_float avx_reg __attribute__((vector_size(AVX_WIDTH * sizeof(mandel_float))));
typedef mandel_int   int_vec __attribute__((vector_size(AVX_WIDTH * sizeof(mandel_int  ))));
typedef __float128 mandel_quad;
typedef __int128   mandel_long;

#define VEC_INIT(val)     {val,val,val,val}
#define VEC_INIT_SEQ(val) {val,val+1,val+2,val+3}

int MANDEL_WIDTH =  800;
int MANDEL_HEIGHT = 600;
mandel_float scale;
mandel_long xoffset;
mandel_long yoffset;

#define SCALE_INIT   (0.5)
#define XOFFSET_INIT (-(MANDEL_WIDTH  / 4))
#define YOFFSET_INIT (-(MANDEL_HEIGHT / 4))
#define MANDEL_ACC_THRESHOLD (1e12)

typedef struct {
	int index;
	int ncores;
	uint32_t *buffer;
} mandel_args;


void mandel_long_print(mandel_long n)
{
	if (n < 0) {
		printf("-");
		n = -n;
	}
	if (n > UINT64_MAX)
	{
		mandel_long leading  = n / 10000000000000000000ULL;
		uint64_t  trailing = n % 10000000000000000000ULL;
		mandel_long_print(leading);
		printf("%.19lu", trailing);
	}
	else
	{
		uint64_t u64 = n;
		printf("%lu", u64);
	}
}


int mandel_compute_double(void *ptr) {
		mandel_args *args = (mandel_args*) ptr;
	uint32_t *buffer = args->buffer;
	int ncores = args->ncores;

	mandel_float width  = scale * MANDEL_WIDTH;
	mandel_float height = scale * MANDEL_HEIGHT;
	mandel_float accuracy = scale * 1000;
	mandel_float wfactor  = width  / accuracy;
	mandel_float hfactor  = height / accuracy;

	mandel_int xstart = xoffset;
	mandel_int xend   = xstart + MANDEL_WIDTH;
	mandel_int ystart = yoffset + args->index;
	mandel_int yend   = ystart + MANDEL_HEIGHT;

	for (mandel_int y = ystart; y < yend; y += ncores) {
		if (y - ystart + args->index >= MANDEL_HEIGHT) {
			break;
		}
		for (mandel_int x = xstart; x < xend; x += AVX_WIDTH) {
			avx_reg xv = VEC_INIT(x);
			avx_reg yv = VEC_INIT(y);
			avx_reg adds = VEC_INIT_SEQ(0);
			avx_reg ar = 2 * wfactor * ((xv + adds) / width  - 0.5);
			avx_reg ai = 2 * hfactor * ( yv         / height - 0.5);
			avx_reg br = VEC_INIT(0.0);
			avx_reg bi = VEC_INIT(0.0);

			int_vec results = VEC_INIT(0);
			for (int i = 0; i < 256; i++) {
				avx_reg br2 = br * br;
				avx_reg bi2 = bi * bi;
				avx_reg square_sums = br2 + bi2;
				int_vec is_finished_and_no_result_yet = ((square_sums >= 4.0) & (results == 0)) & 1;
				results += is_finished_and_no_result_yet * i;

				unsigned int mask = _mm256_movemask_epi8(results != 0);
				if (mask == 0xffffffff) {
					break;
				}

				avx_reg r0 = br2 - bi2;
				avx_reg i0 = 2 * br * bi;
				br = r0;
				bi = i0;
				br += ar;
				bi += ai;
			}
			int index = (y - ystart + args->index) * MANDEL_WIDTH + (x - xstart);
			for (int n = 0; n < AVX_WIDTH; n++) {
				buffer[index + n] = results[n] << 24 | results[n] << 16 | results[n] << 8 | 0;
			}
		}
	}
	return 0;
}

int mandel_compute_quad(void *ptr) {
	mandel_args *args = (mandel_args*) ptr;
	uint32_t *buffer = args->buffer;
	int ncores = args->ncores;

	mandel_quad width  = scale * MANDEL_WIDTH;
	mandel_quad height = scale * MANDEL_HEIGHT;
	mandel_quad accuracy = scale * 1000;
	mandel_quad wfactor  = width  / accuracy;
	mandel_quad hfactor  = height / accuracy;

	mandel_long xstart = xoffset;
	mandel_long xend   = xstart + MANDEL_WIDTH;
	mandel_long ystart = yoffset + args->index*2;
	mandel_long yend   = ystart + MANDEL_HEIGHT;

	for (mandel_long y = ystart; y < yend; y += ncores*2) {
		if (y - ystart + args->index*2 >= MANDEL_HEIGHT) {
			break;
		}
		for (mandel_long x = xstart; x < xend; x += 2) {
			mandel_quad ar = 2 * wfactor * (x / width  - 0.5);
			mandel_quad ai = 2 * hfactor * (y / height - 0.5);
			mandel_quad br = 0.0;
			mandel_quad bi = 0.0;

			int i;
			for (i = 0; i < 256 && (br*br) + (bi*bi) < 4; i++) {
				mandel_quad r0 = br * br - bi * bi;
				mandel_quad i0 = 2 * br * bi;
				br = r0;
				bi = i0;
				br += ar;
				bi += ai;
			}
			int index = (y - ystart + args->index*2) * MANDEL_WIDTH + (x - xstart);
			uint32_t res = i << 24 | i << 16 | i << 8 | 0;
			buffer[index] = res;
			buffer[index+1] = res;
			buffer[index+MANDEL_WIDTH] = res;
			buffer[index+MANDEL_WIDTH+1] = res;
		}
	}
	return 0;
}


void mandel_draw(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture) {
	int width;
	int height;
	SDL_GetWindowSize(window, &width, &height);

	SDL_Rect dst;
	int normalized_width = width * MANDEL_HEIGHT / MANDEL_WIDTH;
	if (normalized_width < height) {
		dst.w = width;
		dst.h = width * MANDEL_HEIGHT / MANDEL_WIDTH;
		dst.x = 0;
		dst.y = (height - dst.h) / 2;
	} else {
		dst.h = height;
		dst.w = height * MANDEL_WIDTH / MANDEL_HEIGHT;
		dst.y = 0;
		dst.x = (width - dst.w) / 2;
	}

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, &dst);
	SDL_RenderPresent(renderer);
}

int mandel_compute_and_draw(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture) {
	static unsigned int total_time = 0;
	static unsigned int ntimes = 0;

	printf("Computing...\n");

	unsigned int start_time, end_time;
	start_time = SDL_GetTicks();

	uint32_t *pixels;
	int pitch;
	if (SDL_LockTexture(texture, NULL, (void**) &pixels, &pitch) < 0) {
		fprintf(stderr, "Could not lock SDL_Texture: %s!\n", SDL_GetError());
		return 1;
	}

	int (*thread_fn)(void*) = scale < MANDEL_ACC_THRESHOLD ? mandel_compute_double : mandel_compute_quad;
	int cores = SDL_GetCPUCount();
	SDL_Thread **threads = malloc(cores * sizeof(SDL_Thread*));
	mandel_args *args = malloc(cores * sizeof(mandel_args));
	for (int n = 0; n < cores; n++) {
		args[n].index = n;
		args[n].buffer = pixels;
		args[n].ncores = cores;
		threads[n] = SDL_CreateThread(thread_fn, NULL, &args[n]);
	}
	for (int n = 0; n < cores; n++) {
		SDL_WaitThread(threads[n], NULL);
	}
	free(threads);
	free(args);

	SDL_UnlockTexture(texture);
	end_time = SDL_GetTicks();
	total_time += end_time - start_time;
	ntimes++;

	mandel_draw(window, renderer, texture);

	system("clear");
	printf("Stats:\n");
	printf("Mode:                   %s\n", scale < MANDEL_ACC_THRESHOLD ? "float64" : "float128");
	printf("Computing-time:         %u ms\n", end_time - start_time);
	printf("Average Computing time: %u ms\n", total_time / ntimes);
	printf("Scale:                  %lg\n", scale);
	printf("X-Coord:                "); mandel_long_print(xoffset); printf("\n");
	printf("Y-Coord:                "); mandel_long_print(yoffset); printf("\n");
	return 0;
}


int main(int argc, char **argv) {
	(void) argc; (void) argv;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Could not init SDL: %s!\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = SDL_CreateWindow("Mandelbrot",
										  SDL_WINDOWPOS_CENTERED,
										  SDL_WINDOWPOS_CENTERED,
										  MANDEL_WIDTH, MANDEL_HEIGHT,
										  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		fprintf(stderr, "Could not create SDL_Window: %s!\n", SDL_GetError());
		return 1;
	}

	int display_index = SDL_GetWindowDisplayIndex(window);
	SDL_DisplayMode dm;
	SDL_GetCurrentDisplayMode(display_index, &dm);
	MANDEL_WIDTH = dm.w;
	MANDEL_HEIGHT = dm.h;

	SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	if (renderer == NULL) {
		fprintf(stderr, "Could not create SDL_Renderer: %s!\n", SDL_GetError());
		return 1;
	}

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, MANDEL_WIDTH, MANDEL_HEIGHT);
	if (texture == NULL) {
		fprintf(stderr, "Could not create SDL_Texture: %s!\n", SDL_GetError());
		return 1;
	}

	scale = SCALE_INIT;
	xoffset = XOFFSET_INIT;
	yoffset = YOFFSET_INIT;
	int mouse_pressed_x = 0;
	int mouse_pressed_y = 0;

	if (mandel_compute_and_draw(window, renderer, texture)) {
		return 1;
	}

	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT: goto while_exit;
			case SDL_WINDOWEVENT: {
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					mandel_draw(window, renderer, texture);
				}
				break;
			}
			case SDL_MOUSEWHEEL: {
				int winw, winh;
				SDL_GetWindowSize(window, &winw, &winh);
				int mousex, mousey;
				SDL_GetMouseState(&mousex, &mousey);
				int n = scale < MANDEL_ACC_THRESHOLD ? 1 : 4;
				while (event.wheel.y < 0 && n--) {
					scale /= 1.15;
					if (scale > 0.5) {
						xoffset -= (mousex / (double) winw) * MANDEL_WIDTH * 0.15;
						yoffset -= (mousey / (double) winh) * MANDEL_HEIGHT * 0.15;
					} else {
						xoffset = XOFFSET_INIT * 2;
						yoffset = YOFFSET_INIT * 2;
					}
					xoffset /= 1.15;
					yoffset /= 1.15;
				}
				while (event.wheel.y > 0 && n--) {
					scale *= 1.15;
					xoffset *= 1.15;
					yoffset *= 1.15;
					if (scale > 0.5) {
						xoffset += (mousex / (double) winw) * MANDEL_WIDTH * 0.15;
						yoffset += (mousey / (double) winh) * MANDEL_HEIGHT * 0.15;
					} else {
						xoffset = XOFFSET_INIT;
						yoffset = YOFFSET_INIT;
					}
				}
				if (mandel_compute_and_draw(window, renderer, texture)) {
					return 1;
				}
				while (SDL_PollEvent(&event));
				break;
			}
			case SDL_KEYDOWN: {
				int key = event.key.keysym.sym;
				if (key == SDLK_UP) {
					yoffset -= MANDEL_HEIGHT / 10;
				} else if (key == SDLK_DOWN) {
					yoffset += MANDEL_HEIGHT / 10;
				} else if (key == SDLK_RIGHT) {
					xoffset += MANDEL_WIDTH / 10;
				} else if (key == SDLK_LEFT) {
					xoffset -= MANDEL_WIDTH / 10;
				} else if (key == SDLK_r) {
					scale = SCALE_INIT;
					xoffset = XOFFSET_INIT;
					yoffset = YOFFSET_INIT;
				} else if (key == SDLK_q) {
					goto while_exit;
				}
				if (mandel_compute_and_draw(window, renderer, texture)) {
					return 1;
				}
				while(SDL_PollEvent(&event));
				break;
			}
			case SDL_MOUSEBUTTONDOWN: {
				mouse_pressed_x = event.button.x;
				mouse_pressed_y = event.button.y;
				break;
			}
			case SDL_MOUSEBUTTONUP: {
				int released_x = event.button.x;
				int released_y = event.button.y;
				xoffset += (mouse_pressed_x - released_x);
				yoffset += (mouse_pressed_y - released_y);
				if (mandel_compute_and_draw(window, renderer, texture)) {
					return 1;
				}
				break;
			}
			default:             break;
		}
	}
	while_exit:

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
