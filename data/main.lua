local Object = require("object")

local cp1252 = {
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
}

local s = twig.surface.new(100, 100)
s:clear(0, 255, 0)

for i = 9, s:get_width() - 10 do
  for j = 9, s:get_height() - 10 do
    s:set(i, j, 255, 255, 0)
  end
end

local rogue = twig.surface.load("data/bit.png")

local font = twig.surface.load("data/font.png")

local br, bg, bb = font:get(0, 0)

local function is_border(x, y)
  if x >= font:get_width() or y >= font:get_height() then
    return true
  end
  local r, g, b = font:get(x, y)
  return r == br and g == bg and b == bb
end

local glyphs = {}
local height = nil
local x = 0
local y = 0
local rowh = 1

local function scan()
  while y < font:get_height() do
    if x >= font:get_width() then
      x = 0
      y = y + rowh
      rowh = 1
    end

    if not is_border(x, y) then
      return
    end

    x = x + 1
  end
end

local total = 256-32

for i = 0, total - 1 do
  scan()

  local gw, gh = 0, 0
  while not is_border(x + gw, y) do
    gw = gw + 1
  end

  while not is_border(x, y + gh) do
    gh = gh + 1
  end

  if not height then
    height = gh
  end

  local code = (i < 96) and (i + 32) or cp1252[i - 96 + 1]
  glyphs[code] = {
    x = x,
    y = y,
    w = gw,
    h = gh,
  }

  x = x + gw

  if gw > rowh then
    rowh = gw
  end
end

for _, g in pairs(glyphs) do
  print(g.x, g.y, g.w, g.h)
end

local function draw_text(s, x, y, text)
  local startx = x
  for _, cp in utf8.codes(text) do
    if cp == 10 then
      x = startx
      y = y + height
    elseif cp ~= 13 then
      local g = glyphs[cp] or glyphs[63]
      font:set_color_mod(255, 0, 0)
      s:blit(font, x, y, g.x, g.y, g.w, g.h)
      x = x + g.w
    end
  end
end

function twig.event(type, a, b, c)
  if type == "key_down" then
    print(a)
  elseif type == "key_up" then
    print(a)
  end
end

function twig.update(dt)

end

function twig.draw(screen)
  screen:clear(0.2 * 255, 0.2 * 255, 0.2 * 255)

  screen:blit(s, 50, 50)

  screen:blit(font, 100, 100)

  draw_text(screen, 10, 10, "Hello!")
end
