#include <math.h>
#include <png.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cmap.h"

#define THREAD_COUNT 16

static int32_t** data= NULL;

typedef struct thread_args {
  double dx, dy;
  uint32_t width, row, iter_max;
} thread_args_t;

static uint32_t resolution = 5000;
static char* dest_file = "out.png";
static pthread_t threads[THREAD_COUNT];
static thread_args_t* thread_args[THREAD_COUNT];
static uint32_t thread_id = 0;

int iterate(double c_re, double c_im, uint32_t iter_max) {
  double re = 0.0, im = 0.0;
  for (uint32_t i = 0; i < iter_max; ++i) {
    double re2 = re * re;
    double im2 = im * im;
    if (re2 + im2 >= 4) {
      return i;
    }
    im = 2 * re * im + c_im;
    re = re2 - im2 + c_re;
  }
  return -1;
}

void* async_row(void* argumnets) {
  thread_args_t* args = argumnets;
  for (uint32_t i = 0; i < args->width; ++i) {
    int iter_n = -1;
    if ((iter_n = iterate((i * args->dx) - 2.0000,
                                (args->row * args->dy) - 1.0000, 10000)) >= 0) {
      data[args->row][i] = iter_n;
      /* uint32_t id = 6 * i; */
      /* uint32_t color = rainbow[iter_n % 256]; */
      /* uint16_t red = ((color >> 16) & 0xff) * 0xff; */
      /* uint16_t green = ((color >> 8) & 0xff) * 0xff; */
      /* uint16_t blue = ((color)&0xff) * 0xff; */
      /* #<{(| uint16_t red = 0xffff * log(1.0/iter_n); |)}># */
      /* #<{(| uint16_t green = 0xffff * log(1.0/iter_n); |)}># */
      /* #<{(| uint16_t blue = 0xffff * log(1.0/iter_n); |)}># */
      /* byte_data[args->row][id] = (red >> 8) & 0xff; */
      /* byte_data[args->row][id + 1] = (red & 0xff); */
      /* byte_data[args->row][id + 2] = (green >> 8) & 0xff; */
      /* byte_data[args->row][id + 3] = (green & 0xff); */
      /* byte_data[args->row][id + 4] = (blue >> 8) & 0xff; */
      /* byte_data[args->row][id + 5] = (blue & 0xff); */
    }
  }
  return NULL;
}

void push_thread(uint32_t row, uint32_t width, double dx, double dy,
                 uint32_t iter_max) {
  if (thread_args[thread_id] != NULL) {
    pthread_join(threads[thread_id], NULL);
    free(thread_args[thread_id]);
  }
  thread_args[thread_id] = (thread_args_t*)malloc(sizeof(thread_args_t));
  thread_args[thread_id]->iter_max = iter_max;
  thread_args[thread_id]->row = row;
  thread_args[thread_id]->width = width;
  thread_args[thread_id]->dx = dx;
  thread_args[thread_id]->dy = dy;
  pthread_create(&threads[thread_id], NULL, async_row, thread_args[thread_id]);
  thread_id = (thread_id + 1) % THREAD_COUNT;
}

void join_threads() {
  for (uint32_t i = 0; i < THREAD_COUNT; ++i) {
    if (thread_args[i] != NULL) {
      pthread_join(threads[i], NULL);
      free(thread_args[i]);
    }
  }
}

bool is_uint32_t(char* str) {
  for (uint32_t i = 0; i < strlen(str); ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  return true;
}
bool ends_with(const char* str, const char* suffix) {
  if (!str || !suffix) return false;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr) return false;
  return !strncmp(str + lenstr - lensuffix, suffix, lensuffix);
}

bool parse_args(int argc, char* argv[]) {
  for (int i = 0; i < argc; ++i) {
    if (is_uint32_t(argv[i])) {
      resolution = strtoul(argv[i], NULL, 10);
    } else if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "--help", 6)) {
      printf("./buddhabrot [--help] [RES] [OUTPUT]\n");
      return false;
    } else if (ends_with(argv[i], ".png")) {
      dest_file = argv[i];
    }
  }
  return true;
}

int save_img(uint32_t off, uint32_t res_x, uint32_t res_y){
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    return -1;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    return -2;
  }
  if (setjmp(png_jmpbuf(png))) {
    return -3;
  }
  char buff[255];
  snprintf(buff, 255, "set/%u.png", off);
  FILE* out = fopen(buff, "w");
  if (!out) {
    return -4;
  }
  uint8_t** byte_data = (uint8_t**)malloc(res_y * sizeof(uint8_t*));
  if (!byte_data) {
    fclose(out);
    return -5;
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    byte_data[i] = (uint8_t*)malloc(6 * res_x * sizeof(uint8_t));
    if (!byte_data[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        free(byte_data[j]);
      }
      free(byte_data);
      fclose(out);
      return -6;
    }
    memset(byte_data[i], 0x00, 6 * res_x * sizeof(uint8_t));
    for(uint32_t j = 0; j < res_x; ++j){
      if(data[i][j] < 0) continue;
      uint32_t id = 6 * j;
      uint32_t color = rainbow[(data[i][j] + off) % 255];
      uint16_t red = ((color >> 16) & 0xff) * 0xff;
      uint16_t green = ((color >> 8) & 0xff) * 0xff;
      uint16_t blue = ((color)&0xff) * 0xff;
      /* uint16_t red = 0xffff * log(1.0/iter_n); */
      /* uint16_t green = 0xffff * log(1.0/iter_n); */
      /* uint16_t blue = 0xffff * log(1.0/iter_n); */
      byte_data[i][id] = (red >> 8) & 0xff;
      byte_data[i][id + 1] = (red & 0xff);
      byte_data[i][id + 2] = (green >> 8) & 0xff;
      byte_data[i][id + 3] = (green & 0xff);
      byte_data[i][id + 4] = (blue >> 8) & 0xff;
      byte_data[i][id + 5] = (blue & 0xff);
    }
  }
  png_init_io(png, out);
  png_set_IHDR(png, info, res_x, res_y, 16, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  png_write_image(png, byte_data);
  png_write_end(png, NULL);
  png_free_data(png, info, PNG_FREE_ALL, -1);
  png_destroy_write_struct(&png, (png_infopp)NULL);
  for (uint32_t i = 0; i < res_y; ++i) {
    free(byte_data[i]);
  }
  free(byte_data);
  fclose(out);
  return 0;
}

int main(int argc, char* argv[]) {
  if (!parse_args(argc, argv)) return 0;
  uint32_t res_x = resolution;
  uint32_t res_y = (2.0 / 3.0) * resolution;
  /* uint32_t res_y = (9.0 / 16.0) * (double)(resolution); */
  data = (int32_t**)malloc(res_y * sizeof(int32_t*));
  if (!data) {
    return -5;
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    data[i] = (int32_t*)malloc(res_x * sizeof(int32_t));
    if (!data[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        free(data[j]);
      }
      free(data);
      return -6;
    }
    for(uint32_t j = 0; j < res_x; ++j){
      data[i][j] = -1;
    }
  }
  double dx = 3.0 / (double)res_x;
  double dy = 2.0 / (double)res_y;
  printf("\n");
  for (uint32_t i = 0; i < res_y; ++i) {
    printf("\033[A>> %u\n", i);
    push_thread(i, res_x, dx, dy, 1000);
  }
  join_threads();
  for(uint32_t i = 0; i < 255; ++i){
    save_img(i, res_x, res_y);
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    free(data[i]);
  }
  free(data);
  return 0;
}
