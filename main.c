#include <math.h>
#include <png.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmap.h"

#define THREAD_COUNT 16

static int64_t** data = NULL;

typedef struct thread_args {
  double sx, sy;
  double dx, dy;
  uint32_t width, row;
  uint64_t iter_max;
} thread_args_t;

static short mode = 0;
static uint32_t res[2] = {1920, 1080};
static char* dest = "out";

static uint32_t max_iter = 5000;

static double center_x = -0.743643887037158704752191506114774;
static double center_y = +0.131825904205311970493132056385139;
static double zoom_level = 1000;
static uint32_t frames = 1000;
static double iter_increase = 1e-4;

static uint32_t red_iter = 5000, green_iter = 5000, blue_iter = 5000;

static pthread_t threads[THREAD_COUNT];
static thread_args_t* thread_args[THREAD_COUNT];
static uint32_t thread_id = 0;
static bool rainbow_color = false;

bool is_uint32_t(char* str) {
  for (uint32_t i = 0; i < strlen(str); ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  return true;
}
bool is_uint32_t_pair(char* str) {
  uint32_t i = 0;
  for (i = 0; i < strlen(str) && str[i] != ':'; ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  ++i;
  for (; i < strlen(str); ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  return true;
}
bool is_uint32_t_triple(char* str) {
  uint32_t i = 0;
  for (i = 0; i < strlen(str) && str[i] != ':'; ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  ++i;
  for (; i < strlen(str) && str[i] != ':'; ++i) {
    if (str[i] > 57 || str[i] < 48) return false;
  }
  ++i;
  for (; i < strlen(str); ++i) {
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
bool starts_with(const char* str, const char* prefix) {
  if (!str || !prefix) return false;
  size_t lenstr = strlen(str);
  size_t lenprefix = strlen(prefix);
  if (lenprefix > lenstr) return false;
  return !strncmp(str, prefix, lenprefix);
}

void print_help() {
  printf("./buddhabrot [--help] {0, 1, 2}\n");
  printf("             0 [RES[:RES]] [x=X] [y=Y] [MAX] [ZOOM] [OUTPUT].png\n");
  printf(
      "             1 [RES[:RES]] [x=X] [y=Y] [ZOOM] [FRAMES] [ITER] [BASE] "
      "[OUTPUT]\n");
  printf("             2 [RES[:RES]] [MAX[:MAX:MAX]] [OUTPUT]\n");
}

bool parse_args(int argc, char* argv[]) {
  uint32_t i = 1;
  if (argc >= i + 1 && is_uint32_t(argv[i])) {
    mode = atoi(argv[i]);
  } else {
    print_help();
    return false;
  }
  ++i;
  if (argc >= i + 1 && is_uint32_t(argv[i])) {
    res[0] = strtoul(argv[i], NULL, 10);
    res[1] = (9.0 / 16.0) * res[0];
  } else if (argc >= i + 1 && is_uint32_t_pair(argv[i])) {
    uint32_t j = 0;
    for (j = 0; j < strlen(argv[i]) && argv[i][j] != ':'; ++j) {
    }
    res[1] = strtoul(argv[i] + j + 1, NULL, 10);
    argv[i][j] = 0;
    res[0] = strtoul(argv[i], NULL, 10);
  } else {
    --i;
  }
  ++i;
  if (mode == 0) {
    dest = "out.png";
    zoom_level = 1.0;
    center_x = -1.0;
    center_y = 0.0;
    if (argc >= i + 1 && starts_with(argv[i], "x=") && is_float(argv[i] + 2)) {
      center_x = atof(argv[i] + 2);
      i++;
    }
    if (argc >= i + 1 && starts_with(argv[i], "y=") && is_float(argv[i] + 2)) {
      center_y = atof(argv[i] + 2);
      i++;
    }
    if (argc >= i + 1 && is_uint32_t(argv[i])) {
      max_iter = strtoul(argv[i], NULL, 10);
      i++;
    }
    if (argc >= i + 1 && is_float(argv[i])) {
      zoom_level = atof(argv[i]);
      i++;
    }
  } else if (mode == 1) {
    max_iter = 1000;
    if (argc >= i + 1 && starts_with(argv[i], "x=") && is_float(argv[i] + 2)) {
      center_x = atof(argv[i] + 2);
      i++;
    }
    if (argc >= i + 1 && starts_with(argv[i], "y=") && is_float(argv[i] + 2)) {
      center_y = atof(argv[i] + 2);
      i++;
    }
    if (argc >= i + 1 && is_float(argv[i])) {
      zoom_level = atof(argv[i]);
      i++;
    }
    if (argc >= i + 1 && is_uint32_t(argv[i])) {
      frames = strtoul(argv[i], NULL, 10);
      i++;
    }
    if (argc >= i + 1 && is_float(argv[i])) {
      iter_increase = atof(argv[i]);
      i++;
    }
    if (argc >= i + 1 && is_uint32_t(argv[i])) {
      max_iter = strtoul(argv[i], NULL, 10);
      i++;
    }
  } else if (mode == 2) {
    dest = "out.png";
    if (argc >= i + 1 && is_uint32_t(argv[i])) {
      red_iter = strtoul(argv[i], NULL, 10);
      green_iter = red_iter;
      blue_iter = red_iter;
      i++;
    } else if (argc >= i + 1 && is_uint32_t_triple(argv[i])) {
      uint32_t j = 0;
      for (j = 0; j < strlen(argv[i]) && argv[i][j] != ':'; ++j) {
      }
      uint32_t k = j + 1;
      for (; k < strlen(argv[i]) && argv[i][k] != ':'; ++k) {
      }
      blue_iter = strtoul(argv[i] + k + 1, NULL, 10);
      argv[i][k] = 0;
      green_iter = strtoul(argv[i] + j + 1, NULL, 10);
      argv[i][j] = 0;
      red_iter = strtoul(argv[i], NULL, 10);
      i++;
    }
  } else {
    print_help();
    return false;
  }
  if (argc >= i + 1) {
    dest = argv[i];
    i++;
  }
  if (argc >= i + 1 && !strncmp(argv[i], "c", 1)) {
    rainbow_color = true;
  }
  return true;
}

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

double fround(double val, uint32_t dec) {
  return floor(val * dec) / (double)dec;
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

void* async_buddha(void* argumnets) {
  thread_args_t* args = argumnets;
  double range = (res[1] / (double)res[0]) * 4.0;
  double cx = -1.5;
  double cy = -range / 2.0;
  double dx = 4.0 / (double)res[0];
  double dy = range / (double)res[1];
  for (uint32_t i = 0; i < args->row; ++i) {
    double c_re = (4.0 * drand48() - 2.0);
    double c_im = (range * drand48() - (range / 2.0));
    double* pts = (double*)malloc(sizeof(double) * 2 * args->iter_max);
    double re = 0.0, im = 0.0;
    for (uint64_t i = 0; i < args->iter_max; ++i) {
      pts[2 * i] = re;
      pts[2 * i + 1] = im;
      double re2 = re * re;
      double im2 = im * im;
      if (re2 + im2 >= 4) {
        for (uint64_t j = 0; j < i; ++j) {
          int32_t px = dx * (pts[2 * i] - cx);
          int32_t py = dy * (pts[2 * i + 1] - cy);
          if (px >= 0 && px < res[0] && py >= 0 && py < res[1]) {
            data[py][px]++;
          }
        }
        break;
      }
      im = 2 * re * im + c_im;
      re = re2 - im2 + c_re;
    }
    free(pts);
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

void push_thread_buddha(uint32_t count, uint64_t iter_max) {
  if (thread_args[thread_id] != NULL) {
    pthread_join(threads[thread_id], NULL);
    free(thread_args[thread_id]);
    thread_args[thread_id] = NULL;
  }
  thread_args[thread_id] = (thread_args_t*)malloc(sizeof(thread_args_t));
  thread_args[thread_id]->iter_max = iter_max;
  thread_args[thread_id]->row = count;
  thread_args[thread_id]->width = 0;
  thread_args[thread_id]->sx = 0;
  thread_args[thread_id]->sy = 0;
  thread_args[thread_id]->dx = 0;
  thread_args[thread_id]->dy = 0;
  pthread_create(&threads[thread_id], NULL, async_buddha,
                 thread_args[thread_id]);
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

uint32_t map(uint32_t v, uint32_t as, uint32_t ae, uint32_t bs, uint32_t be) {
  return (uint32_t)((double)bs +
                    (((double)be - (double)bs) / ((double)ae - (double)as)) *
                        ((double)v - (double)as));
}

int save_img(const char* file, uint32_t res_x, uint32_t res_y) {
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
  FILE* out = fopen(file, "w");
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
      uint16_t red, green, blue;
      if (rainbow_color == true) {
        uint32_t color = rainbow[(data[i][j]) % 255];
        red = ((color >> 16) & 0xff) * 0xff;
        green = ((color >> 8) & 0xff) * 0xff;
        blue = ((color)&0xff) * 0xff;
      } else {
        red = 0xffff * log(1.0 / data[i][j]);
        green = 0xffff * log(1.0 / data[i][j]);
        blue = 0xffff * log(1.0 / data[i][j]);
      }
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

int render() {
  data = (int64_t**)malloc(res[1] * sizeof(int64_t*));
  if (!data) {
    return -5;
  }
  for (uint32_t i = 0; i < res[1]; ++i) {
    data[i] = (int64_t*)malloc(res[0] * sizeof(int64_t));
    if (!data[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        free(data[j]);
      }
      free(data);
      return -6;
    }
    for (uint32_t j = 0; j < res[0]; ++j) {
      data[i][j] = -1;
    }
  }
  double range = (res[1] / (double)res[0]) * 4.0;
  double dx = (4.0 / zoom_level) / (double)res[0];
  double dy = (range / zoom_level) / (double)res[1];
  double sx = center_x - (1.5 / zoom_level);
  double sy = center_y - ((range / 2.0) / zoom_level);
  for (uint32_t i = 0; i < res[1]; ++i) {
    push_thread(i, res[0], sx, sy, dx, dy, max_iter);
  }
  join_threads();
  save_img(dest, res[0], res[1]);
  for (uint32_t i = 0; i < res[1]; ++i) {
    free(data[i]);
  }
  free(data);
  return 0;
}

int zoom_render() {
  uint32_t res_x = res[0];
  uint32_t res_y = res[1];
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
  double dz = log((zoom_level)) / (double)(frames);
  double range = (res[1] / (double)res[0]) * 4.0;
  for (uint32_t z = 0; z < frames; ++z) {
    for (uint32_t i = 0; i < res_y; ++i) {
      for (uint32_t j = 0; j < res_x; ++j) {
        data[i][j] = -1;
      }
    }
    double zoom = exp(dz * z);
    double dx = (4.0 / zoom) / (double)res_x;
    double dy = (range / zoom) / (double)res_y;
    double sx = center_x - (1.5 / zoom);
    double sy = center_y - ((range / 2.0) / zoom);
    uint64_t iter = iter_increase * zoom + max_iter;
    printf("Iter: %u/%u\n", z, frames);
    printf("  Zoom:     %lf\n", zoom);
    printf("  Range:   (%e,%e)\n", dx * res_x, dy * res_y);
    printf("  Max Iter: %lu\n", iter);
    for (uint32_t i = 0; i < res_y; ++i) {
      push_thread(i, res_x, sx, sy, dx, dy, iter);
    }
    join_threads();
    char buff[255];
    snprintf(buff, 255, "%s/%u.png", dest, z);
    save_img(buff, res_x, res_y);
  }
  for (uint32_t i = 0; i < res_y; ++i) {
    free(data[i]);
  }
  free(data);
  return 0;
}

int buddha_render() {
  data = (int64_t**)malloc(res[1] * sizeof(int64_t*));
  if (!data) {
    return -5;
  }
  for (uint32_t i = 0; i < res[1]; ++i) {
    data[i] = (int64_t*)malloc(res[0] * sizeof(int64_t));
    if (!data[i]) {
      for (uint32_t j = 0; j < i; ++j) {
        free(data[j]);
      }
      free(data);
      return -6;
    }
    for (uint32_t j = 0; j < res[0]; ++j) {
      data[i][j] = 0;
    }
  }
  uint32_t iter_block = ceil(red_iter / (double)THREAD_COUNT);
  for (uint32_t i = 0; i < red_iter; i += iter_block) {
    push_thread_buddha(iter_block, max_iter);
  }
  join_threads();
  uint32_t max = 0;
  for (uint32_t i = 0; i < res[1]; ++i) {
    for (uint32_t j = 0; j < res[0]; ++j) {
      max = (max > data[i][j]) ? max : data[i][j];
    }
  }
  printf("MAX: %u\n", max);
  for (uint32_t i = 0; i < res[1]; ++i) {
    for (uint32_t j = 0; j < res[0]; ++j) {
      data[i][j] = map(data[i][j], 0, max, 0, 255);
    }
  }
  save_img(dest, res[0], res[1]);

  for (uint32_t i = 0; i < res[1]; ++i) {
    free(data[i]);
  }
  free(data);
  return 0;
}

int main(int argc, char* argv[]) {
  if (!parse_args(argc, argv)) return -1;
  printf("DEST: %s\n", dest);
  printf("RES:  (%u, %u)\n", res[0], res[1]);
  printf("MODE: %d\n", mode);
  printf("COLOR:%d\n", rainbow_color);
  if (mode == 0) {
    printf("  ITER: %u\n", max_iter);
    printf("  ZOOM: %e\n", zoom_level);
    printf("  X:    %+.20lf\n", center_x);
    printf("  Y:    %+.20lf\n", center_y);
    render();
  } else if (mode == 1) {
    printf("  ZOOM:   %e\n", zoom_level);
    printf("  FRAMES: %u\n", frames);
    printf("  ITER:   %e\n", iter_increase);
    printf("  BASE:   %u\n", max_iter);
    printf("  X:      %+.20lf\n", center_x);
    printf("  Y:      %+.20lf\n", center_y);
    zoom_render();
  } else if (mode == 2) {
    printf("  RED:   %u\n", red_iter);
    printf("  GREEN: %u\n", green_iter);
    printf("  BLUE:  %u\n", blue_iter);
    buddha_render();
  }
  printf("====================\n");
  return 0;
}
