#pragma once

#include "pebble.h"

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
