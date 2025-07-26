print("hello!")

local s = twig.surface.new(100, 100)
s:clear(0, 255, 0)

for i = 9, s:get_width() - 10 do
  for j = 9, s:get_height() - 10 do
    s:set(i, j, 255, 255, 0)
  end
end

local rogue = twig.surface.load("rogue.png")

function twig.update(dt)

end

function twig.draw(screen)
  screen:clear(0.2 * 255, 0.2 * 255, 0.2 * 255)

  screen:blit(s, 50, 50)

  screen:blit(rogue, 100, 100)
end
