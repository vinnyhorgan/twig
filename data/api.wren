foreign class Bitmap {
  foreign construct new(w, h)
  foreign construct load(filename)

  foreign width
  foreign height
}

class Graphics {
  foreign static clip(cx, cy, cw, ch)
  foreign static blit_mode(mode)

  foreign static clear(r, g, b, a)

  static clear(r, g, b) {
    clear(r, g, b, 255)
  }

  foreign static plot(x, y, r, g, b, a)

  static plot(x, y, r, g, b) {
    plot(x, y, r, g, b, 255)
  }

  foreign static line(x0, y0, x1, y1, r, g, b, a)

  static line(x0, y0, x1, y1, r, g, b) {
    line(x0, y0, x1, y1, r, g, b, 255)
  }

  foreign static rect(x, y, w, h, r, g, b, a)

  static rect(x, y, w, h, r, g, b) {
    rect(x, y, w, h, r, g, b, 255)
  }

  foreign static rect_line(x, y, w, h, r, g, b, a)

  static rect_line(x, y, w, h, r, g, b) {
    rect_line(x, y, w, h, r, g, b, 255)
  }

  foreign static blit(bmp, x, y, sx, sy, sw, sh)

  static blit(bmp, x, y) {
    blit(bmp, x, y, 0, 0, bmp.width, bmp.height)
  }

  foreign static blit_alpha(bmp, x, y, sx, sy, sw, sh, alpha)

  static blit_alpha(bmp, x, y) {
    blit_alpha(bmp, x, y, 0, 0, bmp.width, bmp.height, 1.0)
  }

  static blit_alpha(bmp, x, y, sx, sy, sw, sh) {
    blit_alpha(bmp, x, y, sx, sy, sw, sh, 1.0)
  }

  foreign static blit_tint(bmp, x, y, sx, sy, sw, sh, r, g, b, a)

  static blit_tint(bmp, x, y, r, g, b) {
    blit_alpha(bmp, x, y, 0, 0, bmp.width, bmp.height, r, g, b, 255)
  }

  foreign static print(text, x, y, r, g, b, a)

  static print(text, x, y, r, g, b) {
    print(text, x, y, r, g, b, 255)
  }
}
