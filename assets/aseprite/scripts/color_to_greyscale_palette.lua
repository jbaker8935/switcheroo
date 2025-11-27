-- Aseprite Lua Script
-- Converts an indexed sprite's palette into greyscale while keeping indices

local sprite = app.activeSprite
if not sprite then
  return app.alert("No active sprite found!")
end

-- Get the sprite's palette
local palette = sprite.palettes[1] -- assuming the default palette is at index 1

-- Create a new greyscale palette
for i = 0, #palette-1 do
  local color = palette:getColor(i)
  
  -- Calculate luminance using the standard formula:
  -- Y = 0.299 * R + 0.587 * G + 0.114 * B
  local luminance = math.floor(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue)
  
  -- Create greyscale color preserving alpha
  local grey = Color{ r = luminance, g = luminance, b = luminance, a = color.alpha }
  
  -- Replace the color in the palette at the same index
  palette:setColor(i, grey)
end

-- Refresh the sprite to render the greyscale version
app.refresh()
app.alert("Palette converted to greyscale successfully!")