#pragma once

#include "pebble.h"

// Emery (200x228, 202 PPI) and Gabbro (260x260, 200 PPI) are the only platforms
// with pixels to spare in the info blocks: everything else is 175-182 PPI on a
// 144px or 180px screen, where the extra glyphs overflow TEXT_BLOCK_SIZE. Lives
// here rather than in consts.h because quadrant.c needs it too and only this
// header is shared.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define HIGH_DPI_INFO 1
#endif

// The render frame must scale with the platform font (see minimalin.c font
// loading). If it doesn't, the larger fonts overflow the box and word-wrap
// clips the trailing glyph onto a hidden second line, which shows as a "-".
#if defined(PBL_PLATFORM_GABBRO)
  #define TEXT_BLOCK_SIZE GSize(105, 34)  // FONT_NUPE_33, scaled 1.444x from chalk
#elif defined(PBL_PLATFORM_EMERY)
  #define TEXT_BLOCK_SIZE GSize(86, 28)   // FONT_NUPE_28
#else
  #define TEXT_BLOCK_SIZE GSize(70, 23)   // FONT_NUPE_23
#endif

#ifdef HIGH_DPI_INFO
// Optional second line above or below a block's main text. The two lines are
// centered on the quadrant as a group, so the main text shifts by half the
// second line's height when one is present.
//
// Line heights are per-block because the fonts differ: the weather range uses
// FONT_NUPE_18 (a digits-only cut of Nupe, built for emery and gabbro only),
// while anything with letters in it has to fall back to a system font, since
// Nupe carries no alphabet.
  #define SUB_TEXT_HEIGHT_NUPE_18 16
  #define SUB_TEXT_HEIGHT_GOTHIC_14 16
// Both fonts leave leading above their glyphs, so the second line is pulled
// toward the main one to keep the pair visually joined.
  #define SUB_TEXT_LEADING 5
#endif

typedef struct TextBlock TextBlock;

typedef void(*TextBlockUpdateProc)(TextBlock * block);

struct TextBlock {
  Layer * layer;
  GFont font;
  GRect frame;
  GColor color;
  TextBlockUpdateProc update_proc;
  void * context;
  bool enabled;
  bool ready;
  bool updating;
  char text[20];
#ifdef HIGH_DPI_INFO
  GFont sub_font;
  GColor sub_color;
  uint8_t sub_height;
  bool sub_above;
  char sub_text[12];
#endif
};


TextBlock * text_block_create(Layer * parent_layer, const GPoint center, const GFont font);
TextBlock * text_block_destroy(TextBlock * text_block);
void text_block_set_text(TextBlock * text_block, const char * text, const GColor color);
void text_block_set_visible(TextBlock * text_block, const bool visible);
bool text_block_get_visible(const TextBlock * const text_block);
void text_block_set_enabled(TextBlock * text_block, const bool enable);
bool text_block_get_enabled(const TextBlock * const text_block);
void text_block_set_ready(TextBlock * text_block, const bool enable);
bool text_block_get_ready(const TextBlock * const text_block);
void text_block_move(TextBlock * text_block, const GPoint center);
void text_block_set_context(TextBlock * text_block, void * context);
void * text_block_get_context(const TextBlock * const text_block);
void text_block_mark_dirty(TextBlock * text_block);
void text_block_set_update_proc(TextBlock * text_block, TextBlockUpdateProc update_proc);
#ifdef HIGH_DPI_INFO
void text_block_set_sub_font(TextBlock * text_block, const GFont font, const int height, const bool above);
void text_block_set_sub_text(TextBlock * text_block, const char * text, const GColor color);
#endif
// Height the second line adds to the block, 0 when there is none. The quadrant
// layout adds it to the hand-collision box so the hands dodge both lines.
int text_block_sub_height(const TextBlock * const text_block);
