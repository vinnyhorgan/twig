import "api.wren" for Bitmap, Graphics

class Main {
  construct new() {}

  init() {
    System.print("hi!!")

    _b = Bitmap.load("roguelike.png")
  }

  update() {
    Graphics.clear(255, 0, 0)

    Graphics.rect_line(10, 10, 50, 50, 0, 255, 0)

    Graphics.plot(1, 1, 0, 0, 255)
    Graphics.line(100, 50, 200, 160, 0, 255, 255)

    Graphics.blit_tint(_b, 100, 100, 0, 16, 16, 16, 0, 0, 255, 255)

    Graphics.print("Hellooooo", 100, 30, 255, 255, 255, 255)
  }

  mouse_move(x, y) {

  }

  mouse_button(button, pressed) {
    System.print("mouse button " + button.toString + (pressed ? " pressed" : " released"))
  }
}

var Twig = Main.new()
