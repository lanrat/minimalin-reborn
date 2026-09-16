#include <pebble.h>
#include "text_block.h"
#include "geometry.h"

// #define DEBUG 1

static void text_block_update_proc(struct Layer *layer, GContext *ctx){
  TextBlock * text_block = * (TextBlock**) layer_get_data(layer);
  text_block->updating = true;
#ifdef DEBUG
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_rect(ctx, text_block->frame);
#endif
  if(text_block->update_proc != NULL){
    text_block->update_proc(text_block);
  }
  if(text_block_get_ready(text_block) && text_block_get_enabled(text_block)){
    // update_proc above may have added or dropped the second line, so measure
    // it now: with one present, both lines straddle the block's center.
    const int sub_height = text_block_sub_height(text_block);
    GRect frame = text_block->frame;
#ifdef HIGH_DPI_INFO
    frame.origin.y += text_block->sub_above ? sub_height / 2 : -sub_height / 2;
#endif
    graphics_context_set_text_color(ctx, text_block->color);
    graphics_draw_text(ctx, text_block->text, text_block->font, frame, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
#ifdef HIGH_DPI_INFO
    if(sub_height > 0){
      const int sub_y = text_block->sub_above
        ? frame.origin.y - sub_height + SUB_TEXT_LEADING
        : frame.origin.y + frame.size.h - SUB_TEXT_LEADING;
      const GRect sub_frame = GRect(frame.origin.x,
                                    sub_y,
                                    frame.size.w,
                                    sub_height + SUB_TEXT_LEADING);
      graphics_context_set_text_color(ctx, text_block->sub_color);
      graphics_draw_text(ctx, text_block->sub_text, text_block->sub_font, sub_frame, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
#endif
  }
  text_block->updating = false;
}

TextBlock * text_block_create(Layer * parent_layer, const GPoint center, const GFont font){
  TextBlock * text_block = (TextBlock *) calloc(1, sizeof(TextBlock));
  Layer * layer = layer_create_with_data(layer_get_frame(parent_layer), sizeof(TextBlock*));
  TextBlock ** data = (TextBlock**) layer_get_data(layer);
  *data = text_block;
  text_block->layer = layer;
  text_block->font = font;
  text_block->enabled = true;
  text_block->ready = true;
  text_block->updating = false;
  const GSize size = TEXT_BLOCK_SIZE;
  const GRect frame = (GRect) {
    .origin = GPoint(center.x - size.w / 2 , center.y - size.h / 2),
    .size   = size
  };
  text_block->frame = frame;
  layer_set_update_proc(layer, text_block_update_proc);
  layer_add_child(parent_layer, layer);
  return text_block;
}

void text_block_set_context(TextBlock * text_block, void * context){
  text_block->context = context;
}

void * text_block_get_context(const TextBlock * const text_block){
  return text_block->context;
}

TextBlock * text_block_destroy(TextBlock * text_block){
  layer_destroy(text_block->layer);
  free(text_block);
  return NULL;
}

void text_block_set_text(TextBlock * text_block, const char * text, const GColor color){
  strncpy(text_block->text, text, sizeof(text_block->text) - 1);
  text_block->text[sizeof(text_block->text) - 1] = '\0';
  text_block->color = color;
  text_block_mark_dirty(text_block);
}

#ifdef HIGH_DPI_INFO
void text_block_set_sub_font(TextBlock * text_block, const GFont font, const int height, const bool above){
  text_block->sub_font = font;
  text_block->sub_height = height;
  text_block->sub_above = above;
}

void text_block_set_sub_text(TextBlock * text_block, const char * text, const GColor color){
  strncpy(text_block->sub_text, text, sizeof(text_block->sub_text) - 1);
  text_block->sub_text[sizeof(text_block->sub_text) - 1] = '\0';
  text_block->sub_color = color;
  text_block_mark_dirty(text_block);
}
#endif

int text_block_sub_height(const TextBlock * const text_block){
#ifdef HIGH_DPI_INFO
  if(text_block->sub_font != NULL && strlen(text_block->sub_text) != 0){
    return text_block->sub_height;
  }
#endif
  return 0;
}

void text_block_set_visible(TextBlock * text_block, const bool visible){
  layer_set_hidden(text_block->layer, !visible);
  text_block_mark_dirty(text_block);
}

bool text_block_get_visible(const TextBlock * const text_block){
  return text_block_get_enabled(text_block) &&
   strlen(text_block->text) != 0 &&
   !layer_get_hidden(text_block->layer);
}

void text_block_set_ready(TextBlock * text_block, const bool ready){
  text_block->ready = ready;
  text_block_mark_dirty(text_block);
}

bool text_block_get_ready(const TextBlock * const text_block){
  return text_block->ready;
}

void text_block_set_enabled(TextBlock * text_block, const bool enabled){
  text_block->enabled = enabled;
  text_block_mark_dirty(text_block);
}

bool text_block_get_enabled(const TextBlock * const text_block){
  return text_block->enabled;
}

void text_block_move(TextBlock * text_block, const GPoint center){
  text_block->frame = grect_from_center_and_size(center, TEXT_BLOCK_SIZE);
  text_block_mark_dirty(text_block);
}

void text_block_mark_dirty(TextBlock * text_block){
  if(!text_block->updating){
    layer_mark_dirty(text_block->layer);
  }
}

void text_block_set_update_proc(TextBlock * text_block, TextBlockUpdateProc update_proc){
   text_block->update_proc = update_proc;
}
