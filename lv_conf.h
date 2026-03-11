/**
 * LVGL Configuration for JustType
 * For LVGL v9.x
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_HOR_RES_MAX 1024
#define LV_VER_RES_MAX 600
#define LV_COLOR_DEPTH 32

/*====================
   MEMORY SETTINGS
  *====================*/
#define LV_MEM_SIZE (2U * 1024U * 1024U)

/*====================
   HAL SETTINGS
  *====================*/
#define LV_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 130

/*====================
   FONT USAGE
  *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_DEFAULT &lv_font_montserrat_24

/*====================
   WIDGET USAGE
  *====================*/
#define LV_USE_BUTTON 1
#define LV_USE_LABEL 1
#define LV_USE_TEXTAREA 1
#define LV_USE_LIST 1
#define LV_USE_MSGBOX 1
#define LV_USE_KEYBOARD 1
#define LV_LABEL_TEXT_SELECTION 1

/*====================
   LAYOUTS
  *====================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*====================
   THEMES
  *====================*/
#define LV_USE_THEME_DEFAULT 1

/*====================
   DRAW ENGINE
  *====================*/
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#define LV_USE_DRAW_ARM2D 0
#define LV_USE_DRAW_ARM2D_SYNC 0
#define LV_USE_NATIVE_HELIUM_ASM 0

/*====================
   SDL DRIVER CONFIG
  *====================*/
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_BUF_COUNT 1

#endif
