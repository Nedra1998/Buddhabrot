#include <math.h>
#include <png.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cmap.h"

#define THREAD_COUNT 16

static int64_t** data = NULL;

typedef struct thread_args {
  double sx, sy;
  double dx, dy;
  uint32_t width, row;
  uint64_t iter_max;
} thread_args_t;

static uint32_t resolution = 5000;
static double center_x = -0.743643887037158704752191506114774;
static double center_y = +0.131825904205311970493132056385139;
static double zoom_level = 1000;
static uint32_t zoom_steps = 1000;
static char* dest_file = "out.png";
static pthread_t threads[THREAD_COUNT];
static thread_args_t* thread_args[THREAD_COUNT];
static uint32_t thread_id = 0;

int iterate(double c_re, double c_im, uint64_t iter_max) {
  double re = 0.0, im = 0.0;
  for (uint64_t i = 0; i < iter_max; ++i) {
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
    if ((iter_n = iterate((i * args->dx) + args->sx,
                          (args->row * args->dy) + args->sy, args->iter_max)) >=
        0) {
      data[args->row][i] = iter_n;
    }
  }
  return NULL;
}

void push_thread(uint32_t row, uint32_t width, double sx, double sy, double dx,
                 double dy, uint32_t iter_max) {
  if (thread_args[thread_id] != NULL) {
    pthread_join(threads[thread_id], NULL);
    free(thread_args[thread_id]);
    thread_args[thread_id] = NULL;
  }
  thread_args[thread_id] = (thread_args_t*)malloc(sizeof(thread_args_t));
  thread_args[thread_id]->iter_max = iter_max;
  thread_args[thread_id]->row = row;
  thread_args[thread_id]->width = width;
  thread_args[thread_id]->sx = sx;
  thread_args[thread_id]->sy = sy;
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
      thread_args[i] = NULL;
    }
  }
}

bool is_uint32_t(char* str) {
  for (uint32_t i = 0; i < strlen(str); ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  return true;
}
bool is_float(char* str) {
  for (uint32_t i = 0; i < strlen(str); ++i) {
    if ((str[i] > 57 || str[i] < 48) && str[i] != 45 && str[i] != 46 &&
        str[i] != 'e')
      return false;
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
  bool res_arg = false, cx_arg = false, cy_arg = false, zl_arg = false,
       zs_arg = false;
  for (int i = 0; i < argc; ++i) {
    if (is_uint32_t(argv[i])) {
      if (res_arg == false) {
        resolution = strtoul(argv[i], NULL, 10);
        res_arg = true;
      } else if (zl_arg == false) {
        zoom_level = strtoul(argv[i], NULL, 10);
        zl_arg = true;
      } else if (zs_arg == false) {
        zoom_steps = strtoul(argv[i], NULL, 10);
        zs_arg = true;
      } else if (cx_arg == false) {
        center_x = atof(argv[i]);
        cx_arg = true;
      } else if (cy_arg == false) {
        center_y = atof(argv[i]);
        cy_arg = true;
      }
    } else if (is_float(argv[i])) {
      if (zl_arg == false) {
        zoom_level = atof(argv[i]);
        zl_arg = true;
      } else if (cx_arg == false) {
        center_x = atof(argv[i]);
        cx_arg = true;
      } else if (cy_arg == false) {
        center_y = atof(argv[i]);
        cy_arg = true;
      }
    } else if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "--help", 6)) {
      printf("./buddhabrot [--help] [RES] [OUTPUT]\n");
      return false;
    } else if (ends_with(argv[i], ".png")) {
      dest_file = argv[i];
    }else if (i != 0){
      printf("Unrecognized argument \"%s\"\n", argv[i]);
      return false;
    }
  }
  return true;
}

int save_img(uint32_t off, uint32_t res_x, uint32_t res_y) {
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
    for (uint32_t j = 0; j < res_x; ++j) {
      if (data[i][j] < 0) continue;
      uint32_t id = 6 * j;
      /* uint32_t color = rainbow[(data[i][j] + off) % 255]; */
      /* uint16_t red = ((color >> 16) & 0xff) * 0xff; */
      /* uint16_t green = ((color >> 8) & 0xff) * 0xff; */
      /* uint16_t blue = ((color)&0xff) * 0xff; */
      uint16_t red = 0xffff * log(1.0 / data[i][j]);
      uint16_t green = 0xffff * log(1.0 / data[i][j]);
      uint16_t blue = 0xffff * log(1.0 / data[i][j]);
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
  printf(">>%lf,%lf<<\n", center_x, center_y);
  printf(">>%u,%lf,%u<<\n", resolution, zoom_level, zoom_steps);
  uint32_t res_x = resolution;
  /* uint32_t res_y = (2.0 / 3.0) * resolution; */
  uint32_t res_y = (9.0 / 16.0) * resolution;
  data = (int64_t**)malloc(res_y * sizeof(int64_t*));
  if (!data) {
    return -5;
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    data[i] = (int64_t*)malloc(res_x * sizeof(int64_t));
    if (!data[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        free(data[j]);
      }
      free(data);
      return -6;
    }
    for (uint32_t j = 0; j < res_x; ++j) {
      data[i][j] = -1;
    }
  }
  double dz = log((zoom_level)) / (double)(zoom_steps);
  for (uint32_t z = 0; z < zoom_steps; ++z) {
    for (uint32_t i = 0; i < res_y; ++i) {
      for (uint32_t j = 0; j < res_x; ++j) {
        data[i][j] = -1;
      }
    }
    double zoom = exp(dz * z);
    /* double zoom = pow(1.5, (double)(z)); */
    /* double zoom = pow((double)(z + 1), 2.0); */
    double dx = (4.0 / zoom) / (double)res_x;
    double dy = (2.25 / zoom) / (double)res_y;
    double sx = center_x - (1.5 / zoom);
    double sy = center_y - (1.0 / zoom);
    uint64_t iter = 1e-4 * zoom + 1000;
    printf("Iter: %u/%u\n", z, zoom_steps);
    printf("  Zoom:     %lf\n", zoom);
    printf("  Range:   (%e,%e)\n", dx * res_x, dy * res_y);
    printf("  Max Iter: %lu\n", iter);
    for (uint32_t i = 0; i < res_y; ++i) {
      push_thread(i, res_x, sx, sy, dx, dy, iter);
    }
    join_threads();
    save_img(z, res_x, res_y);
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    free(data[i]);
  }
  free(data);
  return 0;
}
