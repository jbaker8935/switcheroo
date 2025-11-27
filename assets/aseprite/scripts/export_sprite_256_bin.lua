-- export_sprite_256_bin.lua
-- Only exports sprite index data.  No palette data.

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
  
  local function getPaletteData(palette)
      local ncolors = #palette
      local res = string.format(";%i\n", ncolors)
  
      for i=0, ncolors-1 do
          local color = palette:getColor(i)
          res = res .. "  .byte   " .. string.format("$%x,$%x,$%x,$%x\n", color.blue, color.green, color.red, color.alpha)  
      end
  
      return res
  end
  
  local function getIndexData(img, x, y, w, h)
      local res = ""
      for y = 0,h-1 do
          for x = 0, w-1 do
              px = img:getPixel(x, y)
              res = res .. string.char(px)
          end

      end
  
  
     return res
  end
  
  local function exportFrame(frm,sFlag)
      if frm == nil then
          frm = 1
      end
  
      local img = Image(sprite.spec)
      img:drawSprite(sprite, frm)
      io.write(getIndexData(img, x, y, sprite.width, sprite.height))

  end
  
  local dlg = Dialog()
  dlg:file{ id = "exportFile",
            label = "File",
            open = false,
            save = true,

            filetypes = {"bin" }}

  dlg:modify{ title = "F256 Export Sprite" }
  dlg:button{ id="ok", text="OK" }
  dlg:button{ id="cancel", text="Cancel" }
  dlg:show()
  local data = dlg.data
  if data.ok then
      local f = io.open(data.exportFile, "wb")
      io.output(f)

      exportFrame(app.frame.frameNumber,1)

  
      io.close(f)
  end