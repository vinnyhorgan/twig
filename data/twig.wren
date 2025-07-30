import "api.wren" for Bitmap, Graphics

class Room {
  construct new() {
    var TILE_SIZE = 16
    var TILES_X = Graphics.width / TILE_SIZE
    var TILES_Y = Graphics.height / TILE_SIZE

    System.print("TILES_X: " + TILES_X.toString + ", TILES_Y: " + TILES_Y.toString)
  }
}

class Main {
  construct new() {}

  init() {
    _currentRoom = Room.new()

    _b = Bitmap.load("roguelike.png")
  }

  update() {
    Graphics.clear(255, 0, 0)

    Graphics.rectLine(10, 10, 50, 50, 0, 255, 0)

    Graphics.plot(1, 1, 0, 0, 255)
    Graphics.line(100, 50, 200, 160, 0, 255, 255)

    Graphics.blitTint(_b, 100, 100, 0, 16, 16, 16, 0, 0, 255, 255)

    Graphics.print("Hellooooo", 100, 30, 255, 255, 255, 255)
  }

  mouse_move(x, y) {

  }

  mouse_button(button, pressed) {
    System.print("mouse button " + button.toString + (pressed ? " pressed" : " released"))
  }

  key(key, pressed) {
    System.print("key " + key + (pressed ? " pressed" : " released"))
  }
}

var Twig = Main.new()
