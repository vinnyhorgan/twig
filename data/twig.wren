import "api" for Bitmap, Graphics
import "random" for Random

class Util {
  static rand=(v) { __rand = v }
  static rand { __rand }

  static hslToRgb(h, s, l) {
    h = h % 360
    s = s.clamp(0, 1)
    l = l.clamp(0, 1)

    var c = (1 - (2 * l - 1).abs) * s
    var x = c * (1 - (((h / 60) % 2) - 1).abs)
    var m = l - c / 2

    var r = 0
    var g = 0
    var b = 0

    if (h < 60) {
      r = c
      g = x
    } else if (h < 120) {
      r = x
      g = c
    } else if (h < 180) {
      g = c
      b = x
    } else if (h < 240) {
      g = x
      b = c
    } else if (h < 300) {
      r = x
      b = c
    } else {
      r = c
      b = x
    }

    return [
      ((r + m) * 255).round,
      ((g + m) * 255).round,
      ((b + m) * 255).round
    ]
  }
}

class Mouse {
  static x { __x }
  static y { __y }

  static x=(value) {
    __x = value
  }

  static y=(value) {
    __y = value
  }
}

class Button {
  construct new(x, y, w, h, text, cb) {
    _x = x
    _y = y
    _w = w
    _h = h
    _text = text
    _cb = cb
  }

  mouseInside() {
    return Mouse.x >= _x && Mouse.x <= _x + _w &&
           Mouse.y >= _y && Mouse.y <= _y + _h
  }

  draw() {
    var tw = Graphics.textWidth(_text)
    var th = Graphics.textHeight(_text)
    var tx = _x + (_w - tw) / 2
    var ty = _y + (_h - th) / 2
    var col = Twig.currentRoomBg

    if (mouseInside()) {
      Graphics.rect(_x, _y, _w, _h, 150, 150, 150)
      Graphics.print(_text, tx, ty, col[0], col[1], col[2])
    } else {
      Graphics.rectLine(_x, _y, _w, _h, 150, 150, 150)
      Graphics.print(_text, tx, ty, 150, 150, 150)
    }
  }

  button(button, pressed) {
    if (button == 1 && pressed && mouseInside()) {
      _cb.call()
    }
  }
}

class Tile {
  construct new(spritesheet, x, y, w, h) {
    _spritesheet = spritesheet
    _x = x
    _y = y
    _w = w
    _h = h
  }

  draw(x, y) {
    Graphics.blitAlpha(_spritesheet, x, y, _x, _y, _w, _h)
  }

  draw(x, y, alpha) {
    Graphics.blitAlpha(_spritesheet, x, y, _x, _y, _w, _h, alpha)
  }
}

class Room {
  construct new(name) {
    _name = name

    var TILE_SIZE = 16
    var TILES_X = Graphics.width / TILE_SIZE
    var TILES_Y = Graphics.height / TILE_SIZE

    _tiles = []

    for (y in 0...TILES_Y) {
      var row = []
      for (x in 0...TILES_X) {
        row.add(null)
      }
      _tiles.add(row)
    }

    var h = Util.rand.int(0, 360)
    var s = Util.rand.float(0.1, 0.2)
    var l = Util.rand.float(0.2, 0.3)
    _bg = Util.hslToRgb(h, s, l)
  }

  name { _name }
  bg { _bg }

  addTile(x, y, tile) {
    _tiles[y][x] = tile
  }

  draw() {
    Graphics.clear(_bg[0], _bg[1], _bg[2])

    var TILE_SIZE = 16
    var TILES_X = Graphics.width / TILE_SIZE
    var TILES_Y = Graphics.height / TILE_SIZE

    for (y in 0...TILES_Y) {
      for (x in 0...TILES_X) {
        var tile = _tiles[y][x]
        if (tile != null) {
          tile.draw(x * TILE_SIZE, y * TILE_SIZE)
        }
      }
    }
  }
}

class Main {
  construct new() {}

  currentRoomBg { _rooms[_currentRoom].bg }

  init() {
    _drawing = false

    _runRoom = 0
    _runX = 10
    _runY = 10

    Util.rand = Random.new()

    _mode = "rooms"

    Mouse.x = 0
    Mouse.y = 0

    _sheet = Bitmap.load("roguelike.png")

    _rooms = []
    _currentRoom = 0

    _currentTile = Tile.new(_sheet, 16 * 4, 16 * 2, 16, 16)

    _rooms.add(Room.new("New Room"))

    _newButton = Button.new(Graphics.width - 40, Graphics.height - 40, 30, 30, "+", Fn.new {
      _rooms.add(Room.new("Room " + (_rooms.count + 1).toString))
      _currentRoom = _rooms.count - 1
    })

    _nextButton = Button.new(Graphics.width - 40, Graphics.height - 15 - Graphics.height / 2, 30, 30, ">", Fn.new {
      _currentRoom = (_currentRoom + 1) % _rooms.count
    })

    _prevButton = Button.new(10, Graphics.height - 15 - Graphics.height / 2, 30, 30, "<", Fn.new {
      _currentRoom = (_currentRoom - 1 + _rooms.count) % _rooms.count
    })

    _editButton = Button.new(10, Graphics.height - 40, 60, 30, "Edit", Fn.new {
      _mode = "edit"
      _drawing = false
    })

    _backButton = Button.new(10, 10, 60, 30, "Back", Fn.new {
      _mode = "rooms"
    })

    _runButton = Button.new(Graphics.width - 80, Graphics.height - 40, 30, 30, "Run", Fn.new {
      _mode = "run"
    })
  }

  update() {
    _rooms[_currentRoom].draw()

    if (_mode == "rooms") {
      _newButton.draw()
      _editButton.draw()
      _runButton.draw()

      if (_currentRoom + 1 < _rooms.count) {
        _nextButton.draw()
      }

      if (_currentRoom - 1 >= 0) {
        _prevButton.draw()
      }

      Graphics.rectLine(10, 10, 100, 20, 150, 150, 150)

      var tw = Graphics.textWidth(_rooms[_currentRoom].name)
      var th = Graphics.textHeight(_rooms[_currentRoom].name)
      var tx = 10 + (100 - tw) / 2
      var ty = 10 + (20 - th) / 2
      Graphics.print(_rooms[_currentRoom].name, tx, ty, 150, 150, 150)
    } else if (_mode == "edit") {
      var tileX = (Mouse.x / 16).floor
      var tileY = (Mouse.y / 16).floor

      if (_drawing) {
        _rooms[_currentRoom].addTile(tileX, tileY, _currentTile)
      } else {
        _currentTile.draw(tileX * 16, tileY * 16, 0.3)
        _backButton.draw()
      }
    } else if (_mode == "run") {
      _rooms[_runRoom].draw()
    }
  }

  mouse_move(x, y) {
    Mouse.x = x
    Mouse.y = y
  }

  mouse_button(button, pressed) {
    if (_mode == "rooms") {
      _newButton.button(button, pressed)
      _editButton.button(button, pressed)
      _runButton.button(button, pressed)

      if (_currentRoom + 1 < _rooms.count) {
        _nextButton.button(button, pressed)
      }

      if (_currentRoom - 1 >= 0) {
        _prevButton.button(button, pressed)
      }
    } else if (_mode == "edit") {
      _backButton.button(button, pressed)

      if (button == 1 && pressed) {
        _drawing = true
      } else if (button == 1 && !pressed) {
        _drawing = false
      }
    }
  }

  key(key, pressed) {
    if (_mode == "run") {
      if (key == "escape") {
        _mode = "rooms"
      }
    }
  }
}

var Twig = Main.new()
