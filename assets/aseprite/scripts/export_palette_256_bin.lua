-- export_sprite_256.lua

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
       res = ""
  
      for i=0, ncolors-1 do
          local color = palette:getColor(i)
          res = res .. string.char(color.blue) .. string.char(color.green) .. string.char(color.red) .. string.char(color.alpha)   
      end
  
      return res
  end
  
  local function getIndexData(img, x, y, w, h)
      local res = ""
      for y = 0,h-1 do
          res = res .. "  .byte   "
          for x = 0, w-1 do
              if x > 0 then
                  res = res ..","
              end
              px = img:getPixel(x, y)
              res = res .. string.format("$%x", px)
          end
          res = res .. "\n"
      end
  
  
     return res
  end
  
  local function exportFrame(frm,sFlag)
      if frm == nil then
          frm = 1
      end
  
      local img = Image(sprite.spec)
      img:drawSprite(sprite, frm)
  
      if frm == 1 or frm == nil or sFlag == 1 then

          io.write(getPaletteData(sprite.palettes[1]))

      end

  end
  
  local dlg = Dialog()
  dlg:file{ id = "exportFile",
            label = "File",
            open = false,
            save = true,
          --filename= p .. fn .. "pip",
            filetypes = {"bin"}}
  dlg:check{ id="onlyCurrentFrame",
             text="Export only current frame",
             selected=false }
  dlg:modify{ title = "F256 Export Sprite" }
  dlg:button{ id="ok", text="OK" }
  dlg:button{ id="cancel", text="Cancel" }
  dlg:show()
  local data = dlg.data
  if data.ok then
      local f = io.open(data.exportFile, "wb")
      io.output(f)
  
      if data.onlyCurrentFrame then
          exportFrame(app.frame.frameNumber,1)
      else
          for i = 1,#sprite.frames do
              exportFrame(i,0)
          end
      end
  
      io.close(f)
  end