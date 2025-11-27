-- export_achievements_bin.lua
-- Exports 16 24x24 binary files from a 96x96 indexed sprite.

local sprite = app.sprite

-- Check constraints
if sprite == nil then
  app.alert("No Sprite...")
  return
end
if sprite.colorMode ~= ColorMode.INDEXED then
  app.alert("Sprite needs to be indexed")
  return
end
if sprite.width ~= 96 or sprite.height ~= 96 then
  app.alert("Sprite must be 96x96 pixels")
  return
end

local function getIndexData(img, startX, startY, w, h)
    local res = ""
    for yy = 0, h-1 do
        for xx = 0, w-1 do
            px = img:getPixel(startX + xx, startY + yy)
            res = res .. string.char(px)
        end
    end
    return res
end

local tileSize = 24
local gridSize = 4
local img = Image(sprite.spec)
img:drawSprite(sprite, app.frame.frameNumber)

for row = 0, gridSize-1 do
  for col = 0, gridSize-1 do
    local index = row * gridSize + col
    local filename = "C:/Users/farme/Documents/Code/Aseprite/switch/achievement_" .. index .. ".bin"
    local f = io.open(filename, "wb")
    io.output(f)
    local data = getIndexData(img, col * tileSize, row * tileSize, tileSize, tileSize)
    io.write(data)
    io.close(f)
  end
end