-- KS0108B LCD Driver (100% Lua version)
--
-- Font restrictions:
--   First char font definition must be space ( char( 32 ) )
--   Chars must be consecutive ASCII definitions
--   Some values must be included in the font definition:
--      width = the font width in pixels  (height is not needed)
--      height_in_bytes = the number of bytes a column pixel def ocupies
--      width_x_height_in_bytes = the product of the above values
--
-- Control Hardware requirements:
--   6 GPIO pins for control (can be distributed in any/several ports)
--   8 GPIO pins for data bus (preferably from the same port)


-- ####### Define control constants and use them on f calls .......

--[[
local pio = pio
local tmr = tmr
local string = string
local error = error
local dofile = dofile
--]]

module( ..., package.seeall )

local X = 0             -- The current position on the line, from 0 (left) to 127
local Y = 0             -- The current position on the column, from 0 (up) to 64

-- ## This structure is here only to help solving the 
-- pio.port.setval( port, 0 ) problem and will not be neede for the final version.
local DB = {}            -- DB[ x ] represents the DBx bit of Display Data byte
local DB_port = pio.P8
DB[ 0 ] = pio.P8_0
DB[ 1 ] = pio.P8_1
DB[ 2 ] = pio.P8_2
DB[ 3 ] = pio.P8_3
DB[ 4 ] = pio.P8_4
DB[ 5 ] = pio.P8_5
DB[ 6 ] = pio.P8_6
DB[ 7 ] = pio.P8_7

-- Pinout           -- EXT - DISP
local RW  = pio.P6_5      -- 10  -   5
local CS1 = pio.P6_2      -- 7   -   15
local CS2 = pio.P6_4      -- 9   -   16
local DI  = pio.P6_0      -- 5   -   4
local E   = pio.P6_1      -- 6   -   6
local V0  = pio.P6_6      -- 11  -   3

local tmrid = 0

local font


-- Init gpio control pins used & module mode/state
function init()
  pio.pin.setdir( pio.OUTPUT, RW, CS1, CS2, DI, E, V0 )
  pio.pin.setval( 0, E )
  tmr.delay( 1, 2000 )
  clear()
  set_start_line( 0 )
--  require( "cvfonts" )   -- Read fonts definitions
  dofile( "/rom/cvfonts.lua" )   -- Read fonts definitions
  setfontsmall()
end


-- Current font definition interface
function setfontsmall()
  font = font5x7
end
function setfontbig()
  font = font10x15
end




-- Auxiliar function to help setting the Display control register.
-- Used to set D/I, R/W and Data Byte values
-- First and second arguments are D/I and R/W values (0 or 1)
-- Third and fourth arguments are CS1 and CS2 values (0 or 1)
-- Fifth argument is the data; It can be either an eight bit number or a table containing
-- the Data Byte value ( Be carefull! This value is "big endian"! )
--   If a value (0 or 1) is set for a bit, this bit is set as output and the status is set (low or high)
--   If the value is 'READ' (ex: DB[ x ] = READ), it is set as input.
-- All control lines are set and then the Enable line is set low. After a short delay, it is set high again.
-- For more information, see the controller's timing characteristics (display datasheet, page 8)
local function set_control_lines( DI_value, RW_value, CS1_value, CS2_value, data )
  pio.pin.setval( RW_value, RW )
  pio.pin.setval( DI_value, DI )
  pio.pin.setval( CS1_value, CS1 )
  pio.pin.setval( CS2_value, CS2 )
  
--[[  if data == 0 then         -- ## Circumvent pio bug ## !!!
    for k, v in pairs( DB ) do
      pio.pin.setdir( pio.OUTPUT, v )
      pio.pin.setval( 0, v )
    end
    --]]
  pio.port.setdir( pio.OUTPUT, DB_port )
  pio.port.setval( data, DB_port )
  --tmr.delay( tmrid, tmr.getmindelay( tmrid ) )                  -- Wait some time... This time is being tested
  pio.pin.setval( 1, E )                    -- Set the Enable low
  --tmr.delay( tmrid, tmr.getmindelay( tmrid ) )                  -- Wait some time... This time is being tested
  pio.pin.setval( 0, E )                    -- Set the Enable pin high
  --tmr.delay( tmrid, tmr.getmindelay( tmrid ) )                  -- Wait some time... This time is being tested
  --tmr.delay( tmrid, time )                  -- Wait again...
  pio.pin.setval( 0, CS1, CS2 )
end



-- Turn the display on/off.
-- RAM data is not affected
function on()
  set_control_lines( 0, 0, 1, 1, 0x3F )
end

function off()
  set_control_lines( 0, 0, 1, 1, 0x3E )
end



-- Move cursor to a screen position
function moveto( x, y )    -- Move to a chosen (x, y) position
-- ## Validators must dance right after tests ##### .......
  local aCS1, aCS2
  if y > 7 or x < 0 then
    error( "Y must be between 0 and 7" )
  end
  if x > 127 or x < 0 then
    error( "X must be between 0 and 127" )
  end
  X = x
  Y = y
  if X < 64 then
    aCS1 = 1
    aCS2 = 0
  else
    aCS1 = 0
    aCS2 = 1
    x = x - 64
  end
  set_control_lines( 0, 0, aCS1, aCS2, 0x40 + x )
  set_control_lines( 0, 0, aCS1, aCS2, 0xB8 + y )
end



--[[ ## No need of this for now ##
function moveright( delta )
  moveto( X + delta, Y )
end
function moveleft( delta )
  moveto( X - delta, Y )
end
function moveup( delta )
  moveto( X, Y - delta )
end
function movedown( delta )
  moveto( X, Y + delta )
end
--]]



-- Clear the display and move to (0, 0) position
function clear()
  off()
  for j = 0, 7 do
    moveto( 0, j )
    moveto( 64, j )
    for i = 0, 63 do
      set_control_lines( 1, 0, 1, 1, 0x00 )
    end
  end
  moveto( 64, 0 )
  moveto( 0, 0 )
  on()
end




-- #### NOT TESTED YET
-- Read the display status.
-- Outputs:
--  BUSY
--  ON
--  RESET
--[[
function get_status()
  set_control_lines( 0, 1, 0, 0, { 0, 0, 0, 0, READ, READ, 0, READ } )
  local busy, on, reset = pio.pin.getval( DB[ 7 ], DB[ 5 ], DB [ 4 ] )
  return busy, on, reset
end
--]]



function set_start_line( line )
  set_control_lines( 0, 0, 1, 1, 0xC0 + line )
end



-- Write strings to the LCD screen
function write( data, x, y )
  local aCS1, aCS2
  x = x or X
  y = y or Y
  moveto( x, y )
  for c = 1, #data do
    if X + font.width > 127 then
      moveto( 0, Y + font.height_in_bytes )
    end
    local char = string.sub( data, c, c )
    x = X
    local charpos = ( font.width_x_height_in_bytes * ( string.byte( char ) - 32 ) ) + 1
    for j = 0, font.height_in_bytes - 1 do
      moveto( x, Y + j )
      for i = 0, font.width - 1 do
        if X < 64 then
          aCS1 = 1
          aCS2 = 0
        else
          aCS1 = 0
          aCS2 = 1
        end
        --moveto( X, Y )
        local pos = charpos + i + ( j * font.width )  -- index char in font data
        set_control_lines( 1, 0, aCS1, aCS2, string.byte( string.sub( font.data, pos, pos ) ) )
        X = X + 1
                    if X == 64 then
                      moveto( X, Y )
                      aCS1 = 0
                      aCS2 = 1
                    end
      end
      
      set_control_lines( 1, 0, aCS1, aCS2, 0 )
      X = X + 1
      
      if X == 64 then
        moveto( X, Y )
      end
    end
    moveto( X, Y - font.height_in_bytes + 1 )
  end
end


