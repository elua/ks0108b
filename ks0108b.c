/** \file ks0108b.c
 * \brief KS0108B C driver for eLua.
 * 
 * This is a platform independent driver based on the KS0108B
 * controller ( 64x64 pixels ).\n
 * This implementation uses 2 of them to form a 128x64 like the
 * WG864A. The documentation of this file is embedded on it using
 * doxygen. for extracting it in a readable form, run `doxygen Doxyfile`
 *
 * Released under MIT license.
 * Author: Marcelo Politzer Couto < mpolitzer.c@gmail.com >
 */

/** \defgroup gKS0108B KS0108B
 * \{
 */

/** \defgroup low_level Hardware Interface Functions
 * \ingroup gKS0108B \{  */

/** Optimization level.\n Set it to '> 0' to put the driver in ROM. */
#define MIN_OPT_LEVEL 2
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrodefs.h"
#include "lrotable.h"
#include "common.h"
#include "platform_conf.h"
#include "font_5_7.h"
#include "font_8_16.h"

/** width of the display in pixels */
#define KS0108B_WIDTH	128

/** Lists the Fonts sizes.\n Used by ks0108b_write function. */
enum KS0108B_FONT 
{
  /** 6x8 */
  KS0108B_SMALL,
  /** 8x16 */
  KS0108B_BIG
};

/** \brief Height of display */
#define KS0108B_HEIGHT	8

/** \brief Comand to turn the display on. */
#define KS0108B_CMD_ON	0x3F
/** \def KS0108B_CMD_OFF
 * \brief Comand to turn the display off.
 */
#define KS0108B_CMD_OFF	0x3E

/** \def KS0108B_CMD_X
 * \brief Internal Usage for ks0108bh_gotox
 * 
 */
#define KS0108B_CMD_X	0x40

/** \def KS0108B_CMD_X_MASK
 * \brief Internal Usage for ks0108bh_gotox
 */
#define KS0108B_CMD_X_MASK	63

/** \def KS0108B_CMD_Y
 * \brief Internal Usage for ks0108bh_gotoy
 */
#define KS0108B_CMD_Y	0xB8

/** \def KS0108B_CMD_Y_MASK
 * \brief Internal Usage for ks0108bh_gotoy
 */
#define KS0108B_CMD_Y_MASK	7

/** \def pin_set
 * \brief Macro to simplify setting pin a on.
 * \arg \c p Pin value from eLua
 */
#define pin_set( p ) \
  do{ \
    platform_pio_op( \
        PLATFORM_IO_GET_PORT( p ), \
        1 << PLATFORM_IO_GET_PIN( p ), \
        PLATFORM_IO_PIN_SET ); \
  }while( 0 )

/** Macro to simplify setting pin a off.
 * \arg \c p Pin value from eLua
 */
#define pin_clear( p ) \
  do{ \
    platform_pio_op( \
        PLATFORM_IO_GET_PORT( p ), \
        1 << PLATFORM_IO_GET_PIN( p ), \
        PLATFORM_IO_PIN_CLEAR ); \
  }while( 0 )

/** Macro to simplify setting pin on, and after that off.
 * \arg \c p Pin value from eLua
 */
#define pin_toogle( p ) \
  do { \
    pin_set( p ); \
    pin_clear( p ); \
  } while( 0 )

/** \def port_setval
 * \brief Macro to simplify setting a port to a given value.
 * \arg \c p Port value from eLua
 * \arg \c val Value to pass to the port
 */
#define port_setval( p, val ) \
  do{ \
    platform_pio_op( \
        PLATFORM_IO_GET_PORT( p ), \
        val, \
        PLATFORM_IO_PORT_SET_VALUE ); \
  }while( 0 )

/** \def pin_as_out
 * \brief Macro to simplify setting a port to a given value.
 * \arg \c L Lua stack pointer
 * \arg \c p Pin value from eLua
 * \arg \c i Index of lua stack pointer
 */
#define pin_as_out( L, p, i ) \
  do{ \
    p = luaL_checkinteger( L, i ); \
    platform_pio_op( \
        PLATFORM_IO_GET_PORT( p ), \
        1 << PLATFORM_IO_GET_PIN( p ), \
        PLATFORM_IO_PIN_DIR_OUTPUT ); \
  }while( 0 )

/** KS0108B data port */
int ks_data,
    /** KS0108B chip select 1 pin */
    ks_cs1,
    /** KS0108B chip select 2 pin */
    ks_cs2,
    /** KS0108B read/write pin */
    ks_rw,
    /** KS0108B data/instruction pin */
    ks_rs,
    /** KS0108B enable pin */
    ks_en,
    /** KS0108B ks_rst pin */
    ks_rst,
    /** KS0108B cached X position of display */
    ks_X,
    /** KS0108B cached Y position of display */
    ks_Y;

    /** \arg \c data write data to the part ks_X points to.
     * This function does not write data to both at the same time.
     */
    static void ks0108bh_write_data( u8 );
    static void ks0108bh_write_cmd( u8 );
    static void ks0108bh_gotox( u8 );
    static void ks0108bh_gotoy( u8 );
    static void ks0108bh_gotoxy( u8, u8 );

static void ks0108bh_write_data( u8 data )
{
  if( ks_X >= KS0108B_WIDTH ) // out of bounds
  {
    ks0108bh_gotox( 0 );
  }
  else if( ks_X < 64 )        // cs1
  {
    pin_set( ks_cs1 );
    pin_clear( ks_cs2 );
  }
  else                        // cs2
  {
    pin_clear( ks_cs1 );
    pin_set( ks_cs2 );
  }

  pin_clear( ks_rw );         // write
  pin_set( ks_rs );           // data
  port_setval( ks_data, data );

  ks_X++;
  pin_toogle( ks_en );
}

/** Write command to first half of display. */
static void ks0108bh_write_cmd_cs1( u8 cmd )
{
  pin_set( ks_cs1 );
  pin_clear( ks_cs2 );
  pin_clear( ks_rw );	// write
  pin_clear( ks_rs );	// command
  port_setval( ks_data, cmd );
  pin_toogle( ks_en );
}

/** Write command to second half of display. */
static void ks0108bh_write_cmd_cs2( u8 cmd )
{
  pin_clear( ks_cs1 );
  pin_set( ks_cs2 );
  pin_clear( ks_rw );	// write
  pin_clear( ks_rs );	// command
  port_setval( ks_data, cmd );
  pin_toogle( ks_en );
}

/** Write a command to the whole display. */
static void ks0108bh_write_cmd( u8 cmd )
{
  pin_set( ks_cs1 );
  pin_set( ks_cs2 );	// both controllers
  pin_clear( ks_rw );	// write
  pin_clear( ks_rs );	// command
  port_setval( ks_data, cmd );
  pin_toogle( ks_en );
}

/** Write 0 to all positions of display. */
static void ks0108bh_clear()
{
  u8 i,j;

  ks0108bh_gotoxy( 0, 0 );
  for( j = 0; j < KS0108B_HEIGHT; j++ )
  {
    for( i = 0; i < KS0108B_WIDTH; i++ )
      ks0108bh_write_data( 0 );

    ks0108bh_gotoxy( 0, ks_Y + 1 );
  }
  ks0108bh_gotoxy( 0, 0 );
}

/** Write 0xFF to all positions of display. */
static void ks0108bh_setall()
{
  u8 i,j;

  ks0108bh_gotoxy( 0, 0 );
  for( j = 0; j < KS0108B_HEIGHT; j++ )
  {
    for( i = 0; i < KS0108B_WIDTH; i++ )
      ks0108bh_write_data( 0xFF );

    ks0108bh_gotoxy( 0, ks_Y + 1 );
  }
  ks0108bh_gotoxy( 0, 0 );

}

/* Implementation note ( gotox ):
 * if we set a gotox( x < 64 ), we need to make the pointer to 
 * the second display point at zero, it will leave a strange
 * white space when we change the display.
 *
 * Wrong:
 * 0         64        128
 *    p1        p2
 *    |         |
 * |---------|---------|
 *
 * if both of the disp pointers went to the the same 'x' position,
 * when ks_X reaches the second display it would start at p2,
 * instead of pos 64, so we would have a hole from 64 to p2. To correct
 * this:
 *
 * Correct:
 * 0         64        128
 *    p1     p2
 *    |      |
 * |---------|---------|
 *
 * We make p2 goto 64 when P is < 64.
 */
/** Goto X position, where: 0 <= x < 128 */
static void ks0108bh_gotox( u8 x )
{
  if( x < KS0108B_WIDTH/2 ){
    ks0108bh_write_cmd_cs1( KS0108B_CMD_X | ( x & KS0108B_CMD_X_MASK ) );
    ks0108bh_write_cmd_cs2( KS0108B_CMD_X | ( 0 & KS0108B_CMD_X_MASK ) );
  }
  else // write to both.
    ks0108bh_write_cmd( KS0108B_CMD_X | ( x & KS0108B_CMD_X_MASK ) );
  ks_X = x;
}

/** Goto Y position, where: 0 <= y < 8.\n The top of the screen is 0. */
static void ks0108bh_gotoy( u8 y ){
  y &= KS0108B_CMD_Y_MASK;
  ks0108bh_write_cmd( KS0108B_CMD_Y | y );
  ks_Y = y;
}

/** Call both gotox and gotoy functions. */
static void ks0108bh_gotoxy( u8 x, u8 y )
{
  ks0108bh_gotox( x );
  ks0108bh_gotoy( y );
}

/** Gets the width based on the declaration of the array.\n
 * For this to work, the array must be a char [][h][w]; where
 * \c w is width
 * \c h is height. \note for an example check the font_5_7.h file. */
#define FONT_WIDTH( name )  ( sizeof( name[0][0] ) / sizeof( name[0][0][0] ) )

/** Same as \ref FONT_WIDTH */
#define FONT_HEIGHT( name ) ( sizeof( name[0]    ) / sizeof( name[0][0]    ) )

/** Write a string of small characters.\n With special encoding:
 * \arg '\\n' means a line break.\n
 * \arg '\\f' means a clear() folowed by a gotoxy(0,0) */
static void ks0108bh_write_small( const char *str )
{
  for( ; *str; str++ ){
    u8 i;
    const char *ch = font_5_7[0][ *str - FONT_5_7_FIRST_CHAR ];

    if( *str == '\n' )
    {
      ks0108bh_gotoxy( 0, ks_Y + 1 );
    }
    else if( *str == '\f' )
    {
      ks0108bh_clear();
      ks0108bh_gotoxy( 0, 0 );
    }

    else if( *str == '\r' )
    {
      ks0108bh_gotox( 0 );
    }
    else // if not a control 
    {
      if( ks_X >= KS0108B_WIDTH - FONT_WIDTH( font_5_7 ) + 1 )
        ks0108bh_gotoxy( 0, ks_Y + 1 );
      for( i = 0; i < FONT_WIDTH( font_5_7 ); i++ )
      {
        ks0108bh_write_data( *ch );
        ch++;
      }
      ks0108bh_write_data( 0 );
    }
  }
}

/** Write a string of big characters.\n With special encoding:
 * \arg '\\n' means a line break.\n
 * \arg '\\f' means a clear() folowed by a gotoxy(0,0) 
 */
static void ks0108bh_write_big( const char *str )
{
  for( ; *str; str++ )
  {
    u8 i, j;
    char *ch;
    switch( *str )
    {
      case '\n': ks0108bh_gotoxy( 0, ks_Y + 2 ); break;
      case '\f': ks0108bh_clear(); ks0108bh_gotoxy( 0, 0 ); break;

      default:
                 if( ks_X >= KS0108B_WIDTH - FONT_WIDTH( font_8_16 ) + 1 )
                   ks0108bh_gotoxy( 0, ks_Y + 2 );

                 for( j = 0; j < FONT_HEIGHT( font_8_16 ); j++ ){
                   ch = font_8_16[ *str - FONT_8_16_FIRST_CHAR ][j];
                   for( i = 0; i < FONT_WIDTH( font_8_16 ); i++ )
                   {
                     ks0108bh_write_data( *ch );
                     ch++;
                   }
                   ks0108bh_gotoxy( ks_X - FONT_WIDTH( font_8_16 ), ks_Y + 1 );
                 }
                 ks0108bh_gotoxy( ks_X + FONT_WIDTH( font_8_16 ),
                     ks_Y - FONT_HEIGHT( font_8_16 ) );
                 break;
    }
  }
}

/** \} */ // Low level subgroup

//////////////////////////////////////////////////////
// High Level Functions

/** 
 * \code
 * ks0108b.init( PORT, CS1, [ CS2 , ] RW, RS, RST )
 * \endcode
 * CS2 is mandataory when KS0108B_DUAL is defined ( 128x64 version ).\n
 * Otherwise It doesnt exist.\n
 *
 * \b Parameters \b in \b order:
 *  \arg    port [pio.port]
 *	\arg    cs1  [pio.pin]
 *	\arg    cs2  [pio.pin]
 *	\arg    rw   [pio.pin]
 *	\arg    rs   [pio.pin]
 *	\arg    rst  [pio.pin]
 *	\return 
 *	\arg None
 */
static int ks0108b_init( lua_State *L )
{
  ks_data = luaL_checkinteger( L, 1 );
  platform_pio_op(
      PLATFORM_IO_GET_PORT( ks_data ),
      1 << PLATFORM_IO_GET_PIN( ks_data ),
      PLATFORM_IO_PORT_DIR_OUTPUT );

  pin_as_out( L, ks_cs1, 2 );
  pin_as_out( L, ks_cs2, 3 );
  pin_as_out( L, ks_rw,  4 );
  pin_as_out( L, ks_rs,  5 );
  pin_as_out( L, ks_en,  6 );
  pin_as_out( L, ks_rst, 7 );

  pin_set( ks_rst );
  return 0;
}

/** \code
 *  ks0108b.write( str, [ sz, [ x, y ] ] )
 *  \endcode
 *  \arg str is a string with the output.\n
 *  This string can contain:\n
 *  '\\n' - line break\n
 *  '\\f' - clear and gotoxy(0,0)\n\n
 *
 *  Optional:\n
 *  \arg sz can be either:\n
 *  ks0108b.SMALL - 6x8\n
 *  ks0108b.BIG   - 8x16\n
 *  \arg x - 1 <= x <= 128
 *  \arg y - 1 <= y <= 8
 *
 *  if \b x and \b y are used, \b sz must be used too.
 */

static int ks0108b_write( lua_State *L )
{
  const char *str = luaL_checkstring( L, 1 );
  u8 x = luaL_optinteger( L, 3, 1 );
  u8 y = luaL_optinteger( L, 4, 1 );
  u8 char_size = luaL_optinteger( L, 2, KS0108B_SMALL );

  if( lua_gettop( L ) > 2 )
    ks0108bh_gotoxy( x - 1, y - 1 );
  switch( char_size )
  {
    case KS0108B_BIG:	ks0108bh_write_big( str ); break;
    default: 			ks0108bh_write_small( str ); break;
  }

  return 0;
}

/** \code
 *  ks0108b.setall()
 *  \endcode
 * Set all pixels and gotoxy( 0, 0 )
 */
static int ks0108b_setall( lua_State *L )
{
  ks0108bh_setall();
  return 0;
}

/** \code
 *  ks0108b.clear()
 *  \endcode
 *  Clear all pixels and gotoxy( 0, 0 )
 */
static int ks0108b_clear( lua_State *L )
{
  ks0108bh_clear();
  return 0;
}

//////////////////////////////////////////////////////
// Commands

/** \code
 *  ks0108b.on()
 *  \endcode
 * Turn the display on. */
static int ks0108b_on( lua_State *L )
{
  ks0108bh_write_cmd( KS0108B_CMD_ON );
  return 0;
}

/** \code
 *  ks0108b.off()
 *  \endcode
 *  Turn the display off. */
static int ks0108b_off( lua_State *L ){
  ks0108bh_write_cmd( KS0108B_CMD_OFF );
  return 0;
}

/** \code
 *  ks0108b.gotoxy( x, y )
 *  \endcode
 *  \arg 1 <= x <= 128
 *  \arg 1 <= y <= 8
 */
static int ks0108b_gotoxy( lua_State *L )
{
  int x = luaL_checkinteger( L, 1 );
  int y = luaL_checkinteger( L, 2 );
  ks0108bh_gotoxy( x-1, y-1 );
  return 0;
}

//////////////////////////////////////////////////////
// Bind

/** Lua bind table */
#define MIN_OPT_LEVEL 2
#include "lrodefs.h" 
const LUA_REG_TYPE ks0108b_map[] = 
{
  // functions:
  { LSTRKEY( "init" ), LFUNCVAL( ks0108b_init ) },
  { LSTRKEY( "write" ), LFUNCVAL( ks0108b_write ) },
  { LSTRKEY( "clear" ), LFUNCVAL( ks0108b_clear ) },
  { LSTRKEY( "setall" ), LFUNCVAL( ks0108b_setall ) },
  { LSTRKEY( "gotoxy" ), LFUNCVAL( ks0108b_gotoxy ) },
  { LSTRKEY( "on" ), LFUNCVAL( ks0108b_on ) },
  { LSTRKEY( "off" ), LFUNCVAL( ks0108b_off ) },

  // constants:
  { LSTRKEY( "SMALL" ), LNUMVAL( KS0108B_SMALL ) },
  { LSTRKEY( "BIG" ), LNUMVAL( KS0108B_BIG ) },
  { LNILKEY, LNILVAL },
};
/** Lua bind function, uf LUA_OPTIMIZE_MEMORY is > 0.
 * This table goes to rom */
LUALIB_API int luaopen_ks0108b( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else
  luaL_register( L, AUXLIB_KS0108B, ks0108b_map );
  return 1;
#endif
}

/** \} */ // KS0108B group
