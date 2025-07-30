#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "lib/miniz.h"
#include "lib/stb_image.h"
#include "lib/wren.h"

#define RES_W 320
#define RES_H 240
#define TIME_PER_FRAME (1.0 / 30.0)

#define EXPAND(X) ((X) + ((X) > 0))

#define CLIP0(CX, X, X2, W) \
  if (X < CX) {             \
    int D = CX - X;         \
    W -= D;                 \
    X2 += D;                \
    X += D;                 \
  }
#define CLIP1(X, DW, W) \
  if (X + W > DW)       \
    W = DW - X;
#define CLIP()                \
  CLIP0(dst->cx, dx, sx, w);  \
  CLIP0(dst->cy, dy, sy, h);  \
  CLIP0(0, sx, dx, w);        \
  CLIP0(0, sy, dy, h);        \
  CLIP1(dx, dst->cx + cw, w); \
  CLIP1(dy, dst->cy + ch, h); \
  CLIP1(sx, src->w, w);       \
  CLIP1(sy, src->h, h);       \
  if (w <= 0 || h <= 0)       \
  return

// EMBEDDED DATA

const unsigned char e_font[] = {
#embed "font.png"
};

const unsigned char e_data[] = {
#embed "../build/data.zip"
};

char* read_data(const char* filename, size_t* size) {
  mz_zip_archive zip = {0};

  if (!mz_zip_reader_init_mem(&zip, e_data, sizeof(e_data), 0)) {
    printf("failed to initialize zip reader\n");
    return nullptr;
  }

  int index = mz_zip_reader_locate_file(&zip, filename, nullptr, 0);
  if (index < 0) {
    printf("file not found in zip: %s\n", filename);
    mz_zip_reader_end(&zip);
    return nullptr;
  }

  size_t data_size;
  void* data = mz_zip_reader_extract_to_heap(&zip, index, &data_size, 0);
  if (!data) {
    printf("failed to extract file: %s\n", filename);
    mz_zip_reader_end(&zip);
    return nullptr;
  }

  mz_zip_reader_end(&zip);

  char* file = malloc(data_size + 1);
  if (!file) {
    printf("failed to allocate memory for file: %s\n", filename);
    mz_free(data);
    return nullptr;
  }

  memcpy(file, data, data_size);
  file[data_size] = '\0';

  if (size) {
    *size = data_size;
  }

  mz_free(data);
  return file;
}

// BITMAP MANAGEMENT

enum {
  KEEP_ALPHA = 0,
  BLEND_ALPHA = 1,
};

typedef struct {
  unsigned char b, g, r, a;
} Color;

typedef struct {
  int w, h;
  int cx, cy, cw, ch;
  Color* data;
  int blit_mode;
} Bitmap;

Color new_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  Color color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = a;
  return color;
}

Bitmap* bmp_create(int w, int h) {
  Bitmap* bmp = (Bitmap*)calloc(1, sizeof(Bitmap));
  bmp->w = w;
  bmp->h = h;
  bmp->cw = -1;
  bmp->ch = -1;
  bmp->data = (Color*)calloc(w * h, sizeof(Color));
  bmp->blit_mode = BLEND_ALPHA;
  return bmp;
}

Bitmap* bmp_load(void* data, int len) {
  Bitmap* bmp = (Bitmap*)calloc(1, sizeof(Bitmap));
  bmp->cw = -1;
  bmp->ch = -1;
  bmp->blit_mode = BLEND_ALPHA;

  unsigned char* img_data = stbi_load_from_memory(data, len, &bmp->w, &bmp->h, nullptr, 4);
  if (!img_data) {
    free(bmp);
    return nullptr;
  }

  bmp->data = (Color*)calloc(bmp->w * bmp->h, sizeof(Color));
  if (!bmp->data) {
    stbi_image_free(img_data);
    free(bmp);
    return nullptr;
  }

  for (int y = 0; y < bmp->h; y++) {
    for (int x = 0; x < bmp->w; x++) {
      int src_i = (y * bmp->w + x) * 4;
      int dst_i = y * bmp->w + x;
      bmp->data[dst_i] = new_color(img_data[src_i], img_data[src_i + 1], img_data[src_i + 2], img_data[src_i + 3]);
    }
  }

  stbi_image_free(img_data);
  return bmp;
}

void bmp_destroy(Bitmap* bmp) {
  free(bmp->data);
  free(bmp);
}

void bmp_clip(Bitmap* bmp, int cx, int cy, int cw, int ch) {
  bmp->cx = cx;
  bmp->cy = cy;
  bmp->cw = cw;
  bmp->ch = ch;
}

void bmp_blit_mode(Bitmap* bmp, int mode) {
  bmp->blit_mode = mode;
}

void bmp_clear(Bitmap* bmp, Color color) {
  int count = bmp->w * bmp->h;
  int n;
  for (n = 0; n < count; n++)
    bmp->data[n] = color;
}

Color bmp_get(Bitmap* bmp, int x, int y) {
  Color empty = {0, 0, 0, 0};
  if (x >= 0 && y >= 0 && x < bmp->w && y < bmp->h)
    return bmp->data[y * bmp->w + x];
  return empty;
}

void bmp_plot(Bitmap* bmp, int x, int y, Color color) {
  int xa, i, a;

  int cx = bmp->cx;
  int cy = bmp->cy;
  int cw = bmp->cw >= 0 ? bmp->cw : bmp->w;
  int ch = bmp->ch >= 0 ? bmp->ch : bmp->h;

  if (x >= cx && y >= cy && x < cx + cw && y < cy + ch) {
    xa = EXPAND(color.a);
    a = xa * xa;
    i = y * bmp->w + x;

    bmp->data[i].r += (unsigned char)((color.r - bmp->data[i].r) * a >> 16);
    bmp->data[i].g += (unsigned char)((color.g - bmp->data[i].g) * a >> 16);
    bmp->data[i].b += (unsigned char)((color.b - bmp->data[i].b) * a >> 16);
    bmp->data[i].a += (bmp->blit_mode) * (unsigned char)((color.a - bmp->data[i].a) * a >> 16);
  }
}

void bmp_line(Bitmap* bmp, int x0, int y0, int x1, int y1, Color color) {
  int sx, sy, dx, dy, err, e2;
  dx = abs(x1 - x0);
  dy = abs(y1 - y0);
  if (x0 < x1)
    sx = 1;
  else
    sx = -1;
  if (y0 < y1)
    sy = 1;
  else
    sy = -1;
  err = dx - dy;

  do {
    bmp_plot(bmp, x0, y0, color);
    e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  } while (x0 != x1 || y0 != y1);
}

void bmp_rect(Bitmap* bmp, int x, int y, int w, int h, Color color) {
  x += 1;
  y += 1;
  w -= 2;
  h -= 2;

  int cx = bmp->cx;
  int cy = bmp->cy;
  int cw = bmp->cw >= 0 ? bmp->cw : bmp->w;
  int ch = bmp->ch >= 0 ? bmp->ch : bmp->h;

  if (x < cx) {
    w += (x - cx);
    x = cx;
  }
  if (y < cy) {
    h += (y - cy);
    y = cy;
  }
  if (x + w > cx + cw) {
    w -= (x + w) - (cx + cw);
  }
  if (y + h > cy + ch) {
    h -= (y + h) - (cy + ch);
  }
  if (w <= 0 || h <= 0)
    return;

  Color* td = &bmp->data[y * bmp->w + x];
  int dt = bmp->w;
  int xa = EXPAND(color.a);
  int a = xa * xa;

  do {
    for (int i = 0; i < w; i++) {
      td[i].r += (unsigned char)((color.r - td[i].r) * a >> 16);
      td[i].g += (unsigned char)((color.g - td[i].g) * a >> 16);
      td[i].b += (unsigned char)((color.b - td[i].b) * a >> 16);
      td[i].a += (bmp->blit_mode) * (unsigned char)((color.a - td[i].a) * a >> 16);
    }
    td += dt;
  } while (--h);
}

void bmp_rect_line(Bitmap* bmp, int x, int y, int w, int h, Color color) {
  int x1, y1;
  if (w <= 0 || h <= 0) {
    return;
  }

  if (w == 1) {
    bmp_line(bmp, x, y, x, y + h, color);
  } else if (h == 1) {
    bmp_line(bmp, x, y, x + w, y, color);
  } else {
    x1 = x + w - 1;
    y1 = y + h - 1;
    bmp_line(bmp, x, y, x1, y, color);
    bmp_line(bmp, x1, y, x1, y1, color);
    bmp_line(bmp, x1, y1, x, y1, color);
    bmp_line(bmp, x, y1, x, y, color);
  }
}

void bmp_blit(Bitmap* dst, Bitmap* src, int dx, int dy, int sx, int sy, int w, int h) {
  int cw = dst->cw >= 0 ? dst->cw : dst->w;
  int ch = dst->ch >= 0 ? dst->ch : dst->h;

  CLIP();

  Color* ts = &src->data[sy * src->w + sx];
  Color* td = &dst->data[dy * dst->w + dx];
  int st = src->w;
  int dt = dst->w;
  do {
    memcpy(td, ts, w * sizeof(Color));
    ts += st;
    td += dt;
  } while (--h);
}

void bmp_blit_tint(Bitmap* dst, Bitmap* src, int dx, int dy, int sx, int sy, int w, int h, Color tint) {
  int cw = dst->cw >= 0 ? dst->cw : dst->w;
  int ch = dst->ch >= 0 ? dst->ch : dst->h;

  CLIP();

  int xr = EXPAND(tint.r);
  int xg = EXPAND(tint.g);
  int xb = EXPAND(tint.b);
  int xa = EXPAND(tint.a);

  Color* ts = &src->data[sy * src->w + sx];
  Color* td = &dst->data[dy * dst->w + dx];
  int st = src->w;
  int dt = dst->w;
  do {
    for (int x = 0; x < w; x++) {
      unsigned r = (xr * ts[x].r) >> 8;
      unsigned g = (xg * ts[x].g) >> 8;
      unsigned b = (xb * ts[x].b) >> 8;
      unsigned a = xa * EXPAND(ts[x].a);
      td[x].r += (unsigned char)((r - td[x].r) * a >> 16);
      td[x].g += (unsigned char)((g - td[x].g) * a >> 16);
      td[x].b += (unsigned char)((b - td[x].b) * a >> 16);
      td[x].a += (dst->blit_mode) * (unsigned char)((ts[x].a - td[x].a) * a >> 16);
    }
    ts += st;
    td += dt;
  } while (--h);
}

void bmp_blit_alpha(Bitmap* dst, Bitmap* src, int dx, int dy, int sx, int sy, int w, int h, float alpha) {
  alpha = (alpha < 0) ? 0 : (alpha > 1 ? 1 : alpha);
  bmp_blit_tint(dst, src, dx, dy, sx, sy, w, h, new_color(0xff, 0xff, 0xff, (unsigned char)(alpha * 255)));
}

// FONTS

typedef struct {
  int code, x, y, w, h;
} Glyph;

typedef struct {
  Bitmap* bitmap;
  int num_glyphs;
  Glyph* glyphs;
} Font;

const char* decode_utf8(const char* text, int* cp) {
  unsigned char c = *text++;
  int extra = 0, min = 0;
  *cp = 0;
  if (c >= 0xf0) {
    *cp = c & 0x07;
    extra = 3;
    min = 0x10000;
  } else if (c >= 0xe0) {
    *cp = c & 0x0f;
    extra = 2;
    min = 0x800;
  } else if (c >= 0xc0) {
    *cp = c & 0x1f;
    extra = 1;
    min = 0x80;
  } else if (c >= 0x80) {
    *cp = 0xfffd;
  } else {
    *cp = c;
  }
  while (extra--) {
    c = *text++;
    if ((c & 0xc0) != 0x80) {
      *cp = 0xfffd;
      break;
    }
    (*cp) = ((*cp) << 6) | (c & 0x3f);
  }
  if (*cp < min) {
    *cp = 0xfffd;
  }
  return text;
}

int cp1252[] = {
    0x20ac, 0xfffd, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152,
    0xfffd, 0x017d, 0xfffd, 0xfffd, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122,
    0x0161, 0x203a, 0x0153, 0xfffd, 0x017e, 0x0178, 0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6,
    0x00a7, 0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af, 0x00b0, 0x00b1, 0x00b2, 0x00b3,
    0x00b4, 0x00b5, 0x00b6, 0x00b7, 0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf, 0x00c0,
    0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7, 0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd,
    0x00ce, 0x00cf, 0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7, 0x00d8, 0x00d9, 0x00da,
    0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df, 0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef, 0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4,
    0x00f5, 0x00f6, 0x00f7, 0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
};

int border(Bitmap* bmp, int x, int y) {
  Color top = bmp_get(bmp, 0, 0);
  Color c = bmp_get(bmp, x, y);
  return (c.r == top.r && c.g == top.g && c.b == top.b) || x >= bmp->w || y >= bmp->h;
}

void scan(Bitmap* bmp, int* x, int* y, int* rowh) {
  while (*y < bmp->h) {
    if (*x >= bmp->w) {
      *x = 0;
      (*y) += *rowh;
      *rowh = 1;
    }
    if (!border(bmp, *x, *y))
      return;
    (*x)++;
  }
}

bool load_glyphs(Font* font) {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int rowh = 1;

  Glyph* g;
  font->num_glyphs = 256 - 32;
  font->glyphs = (Glyph*)calloc(font->num_glyphs, sizeof(Glyph));

  for (int index = 0; index < font->num_glyphs; index++) {
    g = &font->glyphs[index];

    scan(font->bitmap, &x, &y, &rowh);

    if (y >= font->bitmap->h) {
      return false;
    }

    w = h = 0;
    while (!border(font->bitmap, x + w, y)) {
      w++;
    }

    while (!border(font->bitmap, x, y + h)) {
      h++;
    }

    if (index < 96) {
      g->code = index + 32;
    } else {
      g->code = cp1252[index - 96];
    }

    g->x = x;
    g->y = y;
    g->w = w;
    g->h = h;
    x += w;
    if (h != font->glyphs[0].h) {
      return false;
    }

    if (h > rowh) {
      rowh = h;
    }
  }

  for (int i = 1; i < font->num_glyphs; i++) {
    int j = i;
    Glyph g = font->glyphs[i];
    while (j > 0 && font->glyphs[j - 1].code > g.code) {
      font->glyphs[j] = font->glyphs[j - 1];
      j--;
    }
    font->glyphs[j] = g;
  }

  return true;
}

void font_destroy(Font* font) {
  bmp_destroy(font->bitmap);
  free(font->glyphs);
  free(font);
}

Font* font_load(Bitmap* bitmap) {
  Font* font = (Font*)calloc(1, sizeof(Font));
  font->bitmap = bitmap;
  if (!load_glyphs(font)) {
    font_destroy(font);
    return nullptr;
  }
  return font;
}

Glyph* get(Font* font, int code) {
  unsigned lo = 0, hi = font->num_glyphs;
  while (lo < hi) {
    unsigned guess = (lo + hi) / 2;
    if (code < font->glyphs[guess].code)
      hi = guess;
    else
      lo = guess + 1;
  }

  if (lo == 0 || font->glyphs[lo - 1].code != code)
    return &font->glyphs['?' - 32];
  else
    return &font->glyphs[lo - 1];
}

int text_height(Font* font, const char* text) {
  int rowh, h, c;

  h = rowh = get(font, 0)->h;
  while (*text) {
    text = decode_utf8(text, &c);
    if (c == '\n' && *text)
      h += rowh;
  }
  return h;
}

void font_print(Bitmap* dest, Font* font, int x, int y, Color color, const char* text) {
  Glyph* g;
  const char* p;
  int start = x, c;

  p = text;
  while (*p) {
    p = decode_utf8(p, &c);
    if (c == '\r')
      continue;
    if (c == '\n') {
      x = start;
      y += text_height(font, "");
      continue;
    }
    g = get(font, c);
    bmp_blit_tint(dest, font->bitmap, x, y, g->x, g->y, g->w, g->h, color);
    x += g->w;
  }
}

// STATE

typedef struct {
  HWND hwnd;
  HDC hdc;
  BITMAPINFO* bmi;

  int win_w, win_h;
  int dx, dy, dw, dh;

  bool close;

  double tmr_freq;
  double tmr_res;
  LARGE_INTEGER tmr_start;

  Bitmap* bmp;
  Bitmap* font_bmp;
  Font* font;

  WrenVM* vm;
  WrenHandle* twig_handle;
  WrenHandle* mouse_move_handler;
  WrenHandle* mouse_button_handler;
  WrenHandle* key_handler;

  const char* keys[512];
} State;

// WREN API

void wren_write(WrenVM* vm, const char* text) {
  (void)vm;
  printf("%s", text);
}

void wren_error(WrenVM* vm, WrenErrorType error_type, const char* module, const int line, const char* msg) {
  (void)vm;
  switch (error_type) {
    case WREN_ERROR_COMPILE: {
      printf("[%s line %d] [error] %s\n", module, line, msg);
    } break;
    case WREN_ERROR_STACK_TRACE: {
      printf("[%s line %d] in %s\n", module, line, msg);
    } break;
    case WREN_ERROR_RUNTIME: {
      printf("[runtime error] %s\n", msg);
    } break;
  }
}

void load_module_complete(WrenVM* vm, const char* module, WrenLoadModuleResult result) {
  (void)vm;
  (void)module;

  if (result.source) {
    free((void*)result.source);
  }
}

WrenLoadModuleResult wren_load_module(WrenVM* vm, const char* name) {
  (void)vm;

  WrenLoadModuleResult result = {0};
  result.onComplete = load_module_complete;
  result.source = read_data(name, nullptr);
  return result;
}

void wren_bitmap_allocate(WrenVM* vm) {
  wrenEnsureSlots(vm, 1);
  wrenSetSlotNewForeign(vm, 0, 0, sizeof(Bitmap*));
}

void wren_bitmap_finalize(void* data) {
  Bitmap** bmp = (Bitmap**)data;
  bmp_destroy(*bmp);
}

void wren_bitmap_new(WrenVM* vm) {
  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 0);

  int w = (int)wrenGetSlotDouble(vm, 1);
  int h = (int)wrenGetSlotDouble(vm, 2);

  *bmp = bmp_create(w, h);
}

void wren_bitmap_load(WrenVM* vm) {
  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 0);

  const char* filename = wrenGetSlotString(vm, 1);

  size_t size;
  char* data = read_data(filename, &size);

  *bmp = bmp_load(data, size);
  free(data);
}

void wren_bitmap_width(WrenVM* vm) {
  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 0);
  wrenSetSlotDouble(vm, 0, (*bmp)->w);
}

void wren_bitmap_height(WrenVM* vm) {
  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 0);
  wrenSetSlotDouble(vm, 0, (*bmp)->h);
}

void wren_graphics_clip(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  int cx = (int)wrenGetSlotDouble(vm, 1);
  int cy = (int)wrenGetSlotDouble(vm, 2);
  int cw = (int)wrenGetSlotDouble(vm, 3);
  int ch = (int)wrenGetSlotDouble(vm, 4);

  bmp_clip(state->bmp, cx, cy, cw, ch);
}

void wren_graphics_blit_mode(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  const char* mode = wrenGetSlotString(vm, 1);

  if (strcmp(mode, "keep") == 0) {
    bmp_blit_mode(state->bmp, KEEP_ALPHA);
  } else if (strcmp(mode, "blend") == 0) {
    bmp_blit_mode(state->bmp, BLEND_ALPHA);
  }
}

void wren_graphics_clear(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 1);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 2);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 3);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 4);

  bmp_clear(state->bmp, new_color(r, g, b, a));
}

void wren_graphics_plot(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  int x = (int)wrenGetSlotDouble(vm, 1);
  int y = (int)wrenGetSlotDouble(vm, 2);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 3);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 4);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 5);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 6);

  bmp_plot(state->bmp, x, y, new_color(r, g, b, a));
}

void wren_graphics_line(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  int x0 = (int)wrenGetSlotDouble(vm, 1);
  int y0 = (int)wrenGetSlotDouble(vm, 2);

  int x1 = (int)wrenGetSlotDouble(vm, 3);
  int y1 = (int)wrenGetSlotDouble(vm, 4);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 5);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 6);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 7);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 8);

  bmp_line(state->bmp, x0, y0, x1, y1, new_color(r, g, b, a));
}

void wren_graphics_rect(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  int x = (int)wrenGetSlotDouble(vm, 1);
  int y = (int)wrenGetSlotDouble(vm, 2);
  int w = (int)wrenGetSlotDouble(vm, 3);
  int h = (int)wrenGetSlotDouble(vm, 4);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 5);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 6);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 7);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 8);

  bmp_rect(state->bmp, x, y, w, h, new_color(r, g, b, a));
}

void wren_graphics_rect_line(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  int x = (int)wrenGetSlotDouble(vm, 1);
  int y = (int)wrenGetSlotDouble(vm, 2);
  int w = (int)wrenGetSlotDouble(vm, 3);
  int h = (int)wrenGetSlotDouble(vm, 4);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 5);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 6);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 7);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 8);

  bmp_rect_line(state->bmp, x, y, w, h, new_color(r, g, b, a));
}

void wren_graphics_blit(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 1);
  int x = (int)wrenGetSlotDouble(vm, 2);
  int y = (int)wrenGetSlotDouble(vm, 3);
  int sx = (int)wrenGetSlotDouble(vm, 4);
  int sy = (int)wrenGetSlotDouble(vm, 5);
  int sw = (int)wrenGetSlotDouble(vm, 6);
  int sh = (int)wrenGetSlotDouble(vm, 7);

  bmp_blit(state->bmp, *bmp, x, y, sx, sy, sw, sh);
}

void wren_graphics_blit_alpha(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 1);
  int x = (int)wrenGetSlotDouble(vm, 2);
  int y = (int)wrenGetSlotDouble(vm, 3);
  int sx = (int)wrenGetSlotDouble(vm, 4);
  int sy = (int)wrenGetSlotDouble(vm, 5);
  int sw = (int)wrenGetSlotDouble(vm, 6);
  int sh = (int)wrenGetSlotDouble(vm, 7);
  float alpha = (float)wrenGetSlotDouble(vm, 8);

  bmp_blit_alpha(state->bmp, *bmp, x, y, sx, sy, sw, sh, alpha);
}

void wren_graphics_blit_tint(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  Bitmap** bmp = (Bitmap**)wrenGetSlotForeign(vm, 1);
  int x = (int)wrenGetSlotDouble(vm, 2);
  int y = (int)wrenGetSlotDouble(vm, 3);
  int sx = (int)wrenGetSlotDouble(vm, 4);
  int sy = (int)wrenGetSlotDouble(vm, 5);
  int sw = (int)wrenGetSlotDouble(vm, 6);
  int sh = (int)wrenGetSlotDouble(vm, 7);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 8);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 9);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 10);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 11);

  bmp_blit_tint(state->bmp, *bmp, x, y, sx, sy, sw, sh, new_color(r, g, b, a));
}

void wren_graphics_print(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);

  const char* text = wrenGetSlotString(vm, 1);
  int x = (int)wrenGetSlotDouble(vm, 2);
  int y = (int)wrenGetSlotDouble(vm, 3);

  unsigned char r = (unsigned char)wrenGetSlotDouble(vm, 4);
  unsigned char g = (unsigned char)wrenGetSlotDouble(vm, 5);
  unsigned char b = (unsigned char)wrenGetSlotDouble(vm, 6);
  unsigned char a = (unsigned char)wrenGetSlotDouble(vm, 7);

  font_print(state->bmp, state->font, x, y, new_color(r, g, b, a), text);
}

void wren_graphics_width(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);
  wrenSetSlotDouble(vm, 0, state->bmp->w);
}

void wren_graphics_height(WrenVM* vm) {
  State* state = (State*)wrenGetUserData(vm);
  wrenSetSlotDouble(vm, 0, state->bmp->h);
}

WrenForeignMethodFn wren_bind_method(WrenVM* vm,
                                     const char* module,
                                     const char* class_name,
                                     bool is_static,
                                     const char* signature) {
  (void)vm;
  (void)module;
  (void)is_static;

  if (strcmp(class_name, "Bitmap") == 0) {
    if (strcmp(signature, "init new(_,_)") == 0) {
      return wren_bitmap_new;
    } else if (strcmp(signature, "init load(_)") == 0) {
      return wren_bitmap_load;
    } else if (strcmp(signature, "width") == 0) {
      return wren_bitmap_width;
    } else if (strcmp(signature, "height") == 0) {
      return wren_bitmap_height;
    }
  } else if (strcmp(class_name, "Graphics") == 0) {
    if (strcmp(signature, "clip(_,_,_,_)") == 0) {
      return wren_graphics_clip;
    } else if (strcmp(signature, "blitMode(_)") == 0) {
      return wren_graphics_blit_mode;
    } else if (strcmp(signature, "clear(_,_,_,_)") == 0) {
      return wren_graphics_clear;
    } else if (strcmp(signature, "plot(_,_,_,_,_,_)") == 0) {
      return wren_graphics_plot;
    } else if (strcmp(signature, "line(_,_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_line;
    } else if (strcmp(signature, "rect(_,_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_rect;
    } else if (strcmp(signature, "rectLine(_,_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_rect_line;
    } else if (strcmp(signature, "blit(_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_blit;
    } else if (strcmp(signature, "blitAlpha(_,_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_blit_alpha;
    } else if (strcmp(signature, "blitTint(_,_,_,_,_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_blit_tint;
    } else if (strcmp(signature, "print(_,_,_,_,_,_,_)") == 0) {
      return wren_graphics_print;
    } else if (strcmp(signature, "width") == 0) {
      return wren_graphics_width;
    } else if (strcmp(signature, "height") == 0) {
      return wren_graphics_height;
    }
  }

  return nullptr;
}

WrenForeignClassMethods wren_bind_class(WrenVM* vm, const char* module, const char* class_name) {
  (void)vm;
  (void)module;

  WrenForeignClassMethods methods = {0};

  if (strcmp(class_name, "Bitmap") == 0) {
    methods.allocate = wren_bitmap_allocate;
    methods.finalize = wren_bitmap_finalize;
  }

  return methods;
}

// WINDOW PROC

LRESULT CALLBACK wndproc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  State* state = (State*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  LRESULT res = 0;

  switch (message) {
    case WM_PAINT:
      StretchDIBits(state->hdc, state->dx, state->dy, state->dw, state->dh, 0, 0, state->bmp->w, state->bmp->h,
                    state->bmp->data, state->bmi, DIB_RGB_COLORS, SRCCOPY);
      ValidateRect(state->hwnd, nullptr);
      break;

    case WM_SIZE:
      if (wparam == SIZE_MINIMIZED) {
        return res;
      }

      state->win_w = LOWORD(lparam);
      state->win_h = HIWORD(lparam);

      BitBlt(state->hdc, 0, 0, state->win_w, state->win_h, nullptr, 0, 0, BLACKNESS);

      int scale = 0;
      while (true) {
        scale++;
        if (state->bmp->w * scale > state->win_w || state->bmp->h * scale > state->win_h) {
          scale--;
          break;
        }
      }

      if (scale < 1) {
        scale = 1;
      }

      int iw = state->bmp->w * scale;
      int ih = state->bmp->h * scale;
      int ox = (state->win_w - iw) / 2;
      int oy = (state->win_h - ih) / 2;

      state->dx = ox;
      state->dy = oy;
      state->dw = iw;
      state->dh = ih;

      break;

    case WM_DESTROY:
      state->close = true;
      break;

    case WM_MOUSEMOVE:
      int mx = (int)LOWORD(lparam);
      int my = (int)HIWORD(lparam);

      int rel_x = mx - state->dx;
      int rel_y = my - state->dy;

      if (rel_x >= 0 && rel_y >= 0 && rel_x < state->dw && rel_y < state->dh) {
        int bmp_x = rel_x * state->bmp->w / state->dw;
        int bmp_y = rel_y * state->bmp->h / state->dh;

        wrenEnsureSlots(state->vm, 3);
        wrenSetSlotHandle(state->vm, 0, state->twig_handle);
        wrenSetSlotDouble(state->vm, 1, bmp_x);
        wrenSetSlotDouble(state->vm, 2, bmp_y);
        wrenCall(state->vm, state->mouse_move_handler);
      }

      break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
      int button = 0;
      bool pressed = false;

      switch (message) {
        case WM_LBUTTONDOWN:
          pressed = true;
          button = 1;
          break;
        case WM_LBUTTONUP:
          button = 1;
          break;
        case WM_RBUTTONDOWN:
          pressed = true;
          button = 2;
          break;
        case WM_RBUTTONUP:
          button = 2;
          break;
        case WM_MBUTTONDOWN:
          pressed = true;
          button = 3;
          break;
        case WM_MBUTTONUP:
          button = 3;
          break;

        default:
          button = (GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? 5 : 6);
          if (message == WM_XBUTTONDOWN) {
            pressed = true;
          }
      }

      wrenEnsureSlots(state->vm, 3);
      wrenSetSlotHandle(state->vm, 0, state->twig_handle);
      wrenSetSlotDouble(state->vm, 1, button);
      wrenSetSlotBool(state->vm, 2, pressed);
      wrenCall(state->vm, state->mouse_button_handler);

      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      const char* key = nullptr;

      if (wparam == VK_CONTROL) {
        MSG next;
        DWORD time;

        if (lparam & 0x01000000)
          key = "right_control";

        time = GetMessageTime();
        if (PeekMessageW(&next, 0x0, 0, 0, PM_NOREMOVE))
          if (next.message == WM_KEYDOWN || next.message == WM_SYSKEYDOWN || next.message == WM_KEYUP ||
              next.message == WM_SYSKEYUP)
            if (next.wParam == VK_MENU && (next.lParam & 0x01000000) && next.time == time)
              key = "?";

        key = "left_control";
      }

      if (wparam == VK_PROCESSKEY)
        key = "?";

      key = state->keys[HIWORD(lparam) & 0x1FF];
      bool key_pressed = !((lparam >> 31) & 1);

      wrenEnsureSlots(state->vm, 3);
      wrenSetSlotHandle(state->vm, 0, state->twig_handle);
      wrenSetSlotString(state->vm, 1, key ? key : "?");
      wrenSetSlotBool(state->vm, 2, key_pressed);
      wrenCall(state->vm, state->key_handler);

      break;

    default:
      res = DefWindowProc(hwnd, message, wparam, lparam);
  }

  return res;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR p_cmd_line, int n_cmd_show) {
  (void)h_prev_instance;
  (void)p_cmd_line;
  (void)n_cmd_show;

#ifdef DEBUG
  if (!AttachConsole(ATTACH_PARENT_PROCESS))
    AllocConsole();

  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);
  SetConsoleTitle("twig debug console");
#endif

  State state = {0};

  state.keys[0x00B] = "0";
  state.keys[0x002] = "1";
  state.keys[0x003] = "2";
  state.keys[0x004] = "3";
  state.keys[0x005] = "4";
  state.keys[0x006] = "5";
  state.keys[0x007] = "6";
  state.keys[0x008] = "7";
  state.keys[0x009] = "8";
  state.keys[0x00A] = "9";
  state.keys[0x01E] = "a";
  state.keys[0x030] = "b";
  state.keys[0x02E] = "c";
  state.keys[0x020] = "d";
  state.keys[0x012] = "e";
  state.keys[0x021] = "f";
  state.keys[0x022] = "g";
  state.keys[0x023] = "h";
  state.keys[0x017] = "i";
  state.keys[0x024] = "j";
  state.keys[0x025] = "k";
  state.keys[0x026] = "l";
  state.keys[0x032] = "m";
  state.keys[0x031] = "n";
  state.keys[0x018] = "o";
  state.keys[0x019] = "p";
  state.keys[0x010] = "q";
  state.keys[0x013] = "r";
  state.keys[0x01F] = "s";
  state.keys[0x014] = "t";
  state.keys[0x016] = "u";
  state.keys[0x02F] = "v";
  state.keys[0x011] = "w";
  state.keys[0x02D] = "x";
  state.keys[0x015] = "y";
  state.keys[0x02C] = "z";

  state.keys[0x028] = "apostrophe";
  state.keys[0x02B] = "backslash";
  state.keys[0x033] = "comma";
  state.keys[0x00D] = "equal";
  state.keys[0x029] = "grave_accent";
  state.keys[0x01A] = "left_bracket";
  state.keys[0x00C] = "minus";
  state.keys[0x034] = "period";
  state.keys[0x01B] = "right_bracket";
  state.keys[0x027] = "semicolon";
  state.keys[0x035] = "slash";
  state.keys[0x056] = "world_2";

  state.keys[0x00E] = "backspace";
  state.keys[0x153] = "delete";
  state.keys[0x14F] = "end";
  state.keys[0x01C] = "enter";
  state.keys[0x001] = "escape";
  state.keys[0x147] = "home";
  state.keys[0x152] = "insert";
  state.keys[0x15D] = "menu";
  state.keys[0x151] = "page_down";
  state.keys[0x149] = "page_up";
  state.keys[0x045] = "pause";
  state.keys[0x146] = "pause";
  state.keys[0x039] = "space";
  state.keys[0x00F] = "tab";
  state.keys[0x03A] = "caps_lock";
  state.keys[0x145] = "num_lock";
  state.keys[0x046] = "scroll_lock";
  state.keys[0x03B] = "f1";
  state.keys[0x03C] = "f2";
  state.keys[0x03D] = "f3";
  state.keys[0x03E] = "f4";
  state.keys[0x03F] = "f5";
  state.keys[0x040] = "f6";
  state.keys[0x041] = "f7";
  state.keys[0x042] = "f8";
  state.keys[0x043] = "f9";
  state.keys[0x044] = "f10";
  state.keys[0x057] = "f11";
  state.keys[0x058] = "f12";
  state.keys[0x064] = "f13";
  state.keys[0x065] = "f14";
  state.keys[0x066] = "f15";
  state.keys[0x067] = "f16";
  state.keys[0x068] = "f17";
  state.keys[0x069] = "f18";
  state.keys[0x06A] = "f19";
  state.keys[0x06B] = "f20";
  state.keys[0x06C] = "f21";
  state.keys[0x06D] = "f22";
  state.keys[0x06E] = "f23";
  state.keys[0x076] = "f24";
  state.keys[0x038] = "left_alt";
  state.keys[0x01D] = "left_control";
  state.keys[0x02A] = "left_shift";
  state.keys[0x15B] = "left_super";
  state.keys[0x137] = "print_screen";
  state.keys[0x138] = "right_alt";
  state.keys[0x11D] = "right_control";
  state.keys[0x036] = "right_shift";
  state.keys[0x15C] = "right_super";
  state.keys[0x150] = "down";
  state.keys[0x14B] = "left";
  state.keys[0x14D] = "right";
  state.keys[0x148] = "up";

  state.keys[0x052] = "kp_0";
  state.keys[0x04F] = "kp_1";
  state.keys[0x050] = "kp_2";
  state.keys[0x051] = "kp_3";
  state.keys[0x04B] = "kp_4";
  state.keys[0x04C] = "kp_5";
  state.keys[0x04D] = "kp_6";
  state.keys[0x047] = "kp_7";
  state.keys[0x048] = "kp_8";
  state.keys[0x049] = "kp_9";
  state.keys[0x04E] = "kp_add";
  state.keys[0x053] = "kp_decimal";
  state.keys[0x135] = "kp_divide";
  state.keys[0x11C] = "kp_enter";
  state.keys[0x037] = "kp_multiply";
  state.keys[0x04A] = "kp_subtract";

  // SETUP WREN
  WrenConfiguration conf;
  wrenInitConfiguration(&conf);
  conf.writeFn = wren_write;
  conf.errorFn = wren_error;
  conf.loadModuleFn = wren_load_module;
  conf.bindForeignMethodFn = wren_bind_method;
  conf.bindForeignClassFn = wren_bind_class;

  state.vm = wrenNewVM(&conf);

  char* twig_script = read_data("twig.wren", nullptr);
  if (!twig_script) {
    printf("failed to read twig.wren\n");
    wrenFreeVM(state.vm);
    return 1;
  }

  WrenInterpretResult res = wrenInterpret(state.vm, "twig", twig_script);
  free(twig_script);

  if (res != WREN_RESULT_SUCCESS) {
    printf("failed to interpret twig.wren\n");
    wrenFreeVM(state.vm);
    return 1;
  }

  // INIT WINDOW
  SetProcessDPIAware();

  state.bmp = bmp_create(RES_W, RES_H);
  bmp_clear(state.bmp, (Color){30, 30, 30, 255});

  state.font_bmp = bmp_load((void*)e_font, sizeof(e_font));
  state.font = font_load(state.font_bmp);

  int win_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
  win_style |= WS_MAXIMIZEBOX | WS_SIZEBOX;

  HDC hdc = GetDC(nullptr);
  int dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
  int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
  ReleaseDC(nullptr, hdc);

  float sx = dpi_x / (float)USER_DEFAULT_SCREEN_DPI;
  float sy = dpi_y / (float)USER_DEFAULT_SCREEN_DPI;

  RECT rect = {0};
  rect.right = (LONG)(state.bmp->w * sx * 2);
  rect.bottom = (LONG)(state.bmp->h * sy * 2);

  AdjustWindowRect(&rect, win_style, FALSE);

  rect.right -= rect.left;
  rect.bottom -= rect.top;

  int x = (GetSystemMetrics(SM_CXSCREEN) - rect.right) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - rect.bottom + rect.top) / 2;

  WNDCLASS wc = {0};
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wndproc;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = "twig";
  RegisterClass(&wc);

  state.win_w = rect.right;
  state.win_h = rect.bottom;

  state.dw = state.win_w;
  state.dh = state.win_h;

  state.hwnd = CreateWindowEx(0, "twig", "twig", win_style, x, y, state.win_w, state.win_h, nullptr, nullptr,
                              h_instance, nullptr);
  if (!state.hwnd) {
    printf("failed to create window\n");
    bmp_destroy(state.bmp);
    wrenFreeVM(state.vm);
    return 1;
  }

  SetWindowLongPtr(state.hwnd, GWLP_USERDATA, (LONG_PTR)&state);
  ShowWindow(state.hwnd, SW_NORMAL);

  state.hdc = GetDC(state.hwnd);

  state.bmi = (BITMAPINFO*)calloc(1, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3);
  state.bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  state.bmi->bmiHeader.biPlanes = 1;
  state.bmi->bmiHeader.biBitCount = 32;
  state.bmi->bmiHeader.biCompression = BI_BITFIELDS;
  state.bmi->bmiHeader.biWidth = state.bmp->w;
  state.bmi->bmiHeader.biHeight = -(LONG)state.bmp->h;
  state.bmi->bmiColors[0].rgbRed = 0xff;
  state.bmi->bmiColors[1].rgbGreen = 0xff;
  state.bmi->bmiColors[2].rgbBlue = 0xff;

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  state.tmr_freq = (double)freq.QuadPart;
  state.tmr_res = 1.0 / state.tmr_freq;
  QueryPerformanceCounter(&state.tmr_start);

  BitBlt(state.hdc, 0, 0, state.win_w, state.win_h, 0, 0, 0, BLACKNESS);

  // CALL INIT
  wrenSetUserData(state.vm, &state);

  WrenHandle* init_handle = wrenMakeCallHandle(state.vm, "init()");
  WrenHandle* update_handle = wrenMakeCallHandle(state.vm, "update()");

  state.mouse_move_handler = wrenMakeCallHandle(state.vm, "mouse_move(_,_)");
  state.mouse_button_handler = wrenMakeCallHandle(state.vm, "mouse_button(_,_)");
  state.key_handler = wrenMakeCallHandle(state.vm, "key(_,_)");

  wrenEnsureSlots(state.vm, 1);
  wrenGetVariable(state.vm, "twig", "Twig", 0);
  state.twig_handle = wrenGetSlotHandle(state.vm, 0);

  wrenSetSlotHandle(state.vm, 0, state.twig_handle);
  res = wrenCall(state.vm, init_handle);
  if (res != WREN_RESULT_SUCCESS) {
    printf("failed to call init()\n");
    goto cleanup;
  }

  // MAIN LOOP
  while (true) {
    // CALL UPDATE
    wrenSetSlotHandle(state.vm, 0, state.twig_handle);
    res = wrenCall(state.vm, update_handle);
    if (res != WREN_RESULT_SUCCESS) {
      printf("failed to call update()\n");
      goto cleanup;
    }

    if (state.close) {
      break;
    }

    InvalidateRect(state.hwnd, nullptr, TRUE);
    SendMessage(state.hwnd, WM_PAINT, 0, 0);

    MSG msg;
    while (!state.close && PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // SLEEP
    while (true) {
      LARGE_INTEGER now;
      QueryPerformanceCounter(&now);
      double elapsed = (now.QuadPart - state.tmr_start.QuadPart) * state.tmr_res;

      if (elapsed >= TIME_PER_FRAME) {
        state.tmr_start = now;
        break;
      } else if (TIME_PER_FRAME - elapsed > 2.0 / 1000.0) {
        timeBeginPeriod(1);
        Sleep(1);
        timeEndPeriod(1);

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);

          if (state.close) {
            break;
          }
        }
      }
    }
  }

cleanup:
  wrenReleaseHandle(state.vm, init_handle);
  wrenReleaseHandle(state.vm, update_handle);
  wrenReleaseHandle(state.vm, state.twig_handle);
  wrenReleaseHandle(state.vm, state.mouse_move_handler);
  wrenReleaseHandle(state.vm, state.mouse_button_handler);
  wrenReleaseHandle(state.vm, state.key_handler);
  wrenFreeVM(state.vm);

  bmp_destroy(state.bmp);
  free(state.bmi);
  ReleaseDC(state.hwnd, state.hdc);
  DestroyWindow(state.hwnd);

  return res == WREN_RESULT_SUCCESS ? 0 : 1;
}
