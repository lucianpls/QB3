/*
  Prototype use of QB3 library in wasm, with emscripten
  This is a simple test program that reads a PPM image, encodes it and then decodes it
  The image is then displayed in a window

  Once libQB3.a is built using emscripten and a test ppm image is available,
  use the following command do compile this program into a runnable html file:

  emcc -O2 -o world.html world.cpp libQB3.a --preload-file Image.ppm -sUSE_SDL=2
  
  Current performance is 300MB/sec encode and 270MB/sec decode, RGB 8 bit data
*/

#include "../include/QB3.h"
#include <cstdio>
#include <cstdlib>
#include <emscripten.h>

#include <SDL.h>

int read_ppm_header(FILE *f, int &x, int &y, int &zmax) {
	char line[1024];
	char *v = fgets(line, 1024, f);
	if (v[0] != 'P' || v[1] != '6') return 1; // Error
	v = fgets(line, 1024, f);
	sscanf(v, "%d %d", &x, &y);
	v = fgets(line, 1024, f);
	sscanf(v, "%d", &zmax);
	return 0;
}

// Display in a 1024 * 1024 window, image y has to be at least 1024
void display_data(int x, char * data) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
		printf("Can't initialize SDL\n");
	// if (IMG_Init(IMG_INIT_PNG) == 0)
	// 	printf("Error SDL2_Image Initialization\n");

	SDL_Window *window = SDL_CreateWindow("Display",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 1024, SDL_WINDOW_OPENGL
		);
	if (!window) printf("Create window failed\n");

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Texture *texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 1024, 1024);
	if (!texture)
		printf("Can't create texture\n");

	if (SDL_UpdateTexture(texture, NULL, data, x * 3))
		printf("Can't update texture\n");
	SDL_RenderClear(renderer);
	// This scales it to full size, which is a mistake
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	// Should clean up the window too
}

// Single image decode and display
int main2(int argc, char **argv) {
	FILE *f = fopen("Image.qb3", "rb");
	if (!f) {
		printf("Didn't work\n");
		return 1;
	}

	int x, y, bands;
	fseek(f, 0, SEEK_END);
	auto fsize = ftell(f);
	rewind(f);
	auto buffer = malloc(fsize);
	fread(buffer, fsize, 1, f);
	size_t image_size[3]; // x, y and bands
	auto qdec = qb3_read_start(buffer, size_t(fsize), image_size);
	x = image_size[0];
	y = image_size[1];
	bands = image_size[2];
	printf("Image is %dx%d@%d\n", x, y, bands);
	qb3_read_info(qdec);
	auto tp = qb3_get_type(qdec);
	if (tp != QB3_U8) {
		free(buffer);
		qb3_destroy_decoder(qdec);

		printf("Not byte data\n");
		return 1;
	}

	auto raw_size = qb3_decoded_size(qdec);
	char *raw_buffer = (char *)malloc(raw_size);
	// Final decode
	qb3_read_data(qdec, raw_buffer);
	qb3_destroy_decoder(qdec);
	display_data(x, raw_buffer);
	return 0;
}

int main(int argc, char **argv) {
	FILE *f=fopen("Image.ppm", "rb");
	
	int x, y, max_val;
	read_ppm_header(f, x, y, max_val);
	printf("%d %d %d\n", x, y, max_val);
	int raw_size = x * y * 3;
	printf("Raw size %d\n", raw_size);
	char *data = static_cast<char *>(malloc(x * y * 3));
	fread(data, 3 * x, y, f);
	qb3_dtype tp = qb3_dtype::QB3_U8;
	auto qenc = qb3_create_encoder(x, y, 3, tp);
	auto maxsz = qb3_max_encoded_size(qenc);
	char *outbuff = static_cast<char *>(malloc(maxsz));
	qb3_destroy_encoder(qenc);
	int loops = 50;
	auto C = 1e-3 * raw_size * loops;
	// Twice, first is warmup then actual
	for (int j = 0; j < 2; j++)
	{
		double encode_time(0), decode_time(0);
		for (int i = 0; i < loops; i++)
		{
			// printf("Loop %d %f\n", i, encode_time);
			qenc = qb3_create_encoder(x, y, 3, tp);
			auto start = emscripten_get_now();
			qb3_set_encoder_mode(qenc, QB3M_FTL);
			auto outsize = qb3_encode(qenc, data, outbuff);
			auto stop = emscripten_get_now();
			encode_time += stop - start;
			// printf("Compressed to %lu\n", outsize);
			// printf("Encoding took %f\n", stop - start);
			// Let's try decompression too
			qb3_destroy_encoder(qenc);

			//
			size_t image_size[3];
			auto qdec = qb3_read_start(outbuff, outsize, image_size);
			x = image_size[0];
			y = image_size[1];
			max_val = image_size[2];
			// printf("Got size %u %u %u\n", x, y, max_val);
			qb3_read_info(qdec);
			start = emscripten_get_now();
			if (raw_size != qb3_decoded_size(qdec))
				printf("Size error on decode\n");
			if (!qb3_read_data(qdec, data))
				printf("decode error\n");
			stop = emscripten_get_now();
			// printf("Decoding took %f ms\n", stop - start);
			qb3_destroy_decoder(qdec);
			decode_time += stop - start;
		}
		printf("Time (ms)      Encode: %f, Decode: %f\n", encode_time / loops, decode_time / loops);
		printf("Speed (MB/sec) Encode: %f, Decode: %f\n", C / encode_time, C / decode_time);
	}

	free(outbuff);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
		printf("Can't initialize SDL\n");
	// if (IMG_Init(IMG_INIT_PNG) == 0)
	// 	printf("Error SDL2_Image Initialization\n");

	SDL_Window *window = SDL_CreateWindow("First program",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x, y, SDL_WINDOW_OPENGL
		);
	if (!window) printf("Create window failed\n");

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	SDL_Texture *texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 1024, 1024);
	if (!texture)
		printf("Can't create texture\n");

	if (SDL_UpdateTexture(texture, NULL, data, x * 3))
		printf("Can't update texture\n");
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	free(data);
	
	return 0;
}
