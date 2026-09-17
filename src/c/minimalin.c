#include <pebble.h>
#include "consts.h"
#include "config.h"
#include "text_block.h"
#include "quadrant.h"
#include "messenger.h"
#include "minimalin.h"
#include "geometry.h"

// #define d(string, ...) APP_LOG (APP_LOG_LEVEL_DEBUG, string, ##__VA_ARGS__)
// #define e(string, ...) APP_LOG (APP_LOG_LEVEL_ERROR, string, ##__VA_ARGS__)
// #define i(string, ...) APP_LOG (APP_LOG_LEVEL_INFO, string, ##__VA_ARGS__)

typedef enum {
  AppKeyMinuteHandColor = 0,
  AppKeyHourHandColor,
  AppKeyDateDisplayed,
  AppKeyBluetoothIcon,
  AppKeyRainbowMode,
  AppKeyBackgroundColor,
  AppKeyTimeColor,
  AppKeyInfoColor,
  AppKeyTemperatureUnit,
  AppKeyRefreshRate,
  AppKeyWeatherEnabled,
  AppKeyConfig,
  AppKeyWeatherTemperature,
  AppKeyWeatherIcon,
  AppKeyWeatherFailed,
  AppKeyWeatherRequest,
  AppKeyJsReady,
  AppKeyVibrateOnTheHour,
  AppKeyMilitaryTime,
  AppKeyHealthEnabled,
  AppKeyBatteryDisplayedAt,
  AppKeyQuietTimeVisible,
  AppKeyWeatherHigh,
  AppKeyWeatherLow,
  AppKeyExtraDetail,
  AppKeyDistanceUnit
} AppKey;

typedef enum {
  PersistKeyConfig = 0,
  PersistKeyWeather
} PersistKey;

// Appended fields stay at the end: a blob persisted by an older version is
// shorter, so persist_read_data leaves has_range (and the range itself) zeroed.
typedef struct {
  int32_t timestamp;
  int8_t icon;
  int8_t temperature;
  int8_t high;
  int8_t low;
  uint8_t has_range;
} __attribute__((__packed__)) Weather;

typedef struct {
  Config * config;
  Weather weather;
  bool reset_weather;
  int steps;
  int distance_meters;
  bool bluetooth_connected;
  BatteryChargeState charge_state;
  tm * time;
} Context;


static Context s_context;

static Window * s_main_window;
static Layer * s_root_layer;
static GRect s_root_layer_bounds;
static GPoint s_center;
// Height in px of the system overlay (Timeline Quick View) covering the bottom
// of the screen. 0 when nothing is obstructing us.
static int s_obstruction_height;

static TextBlock * s_weather_info;
static TextBlock * s_date_info;
static TextBlock * s_steps_info;
static TextBlock * s_watch_info;
static TextBlock * s_hour_text;
static TextBlock * s_minute_text;

static Layer * s_tick_layer;

static GBitmap * s_rainbow_bitmap;
static Layer * s_minute_hand_layer;
static Layer * s_hour_hand_layer;
static RotBitmapLayer * s_rainbow_hand_layer;
static Layer * s_center_circle_layer;

static Quadrants * s_quadrants;

static Config * s_config;
static Messenger * s_messenger;

static AppTimer * s_weather_request_timer;
static int s_weather_request_timeout;

static int s_js_ready;

static GFont s_font;
#ifdef HIGH_DPI_INFO
static GFont s_sub_font;
#endif

static tm * s_current_time;

static void schedule_weather_request(int timeout);
static void mark_dirty_minute_hand_layer();
static void fetch_health(Context * const context);
static void update_watch_info_layer_visibility();

static void update_current_time() {
  const time_t temp = time(NULL);
  s_current_time = localtime(&temp);
  s_context.time = s_current_time;
}

// Messenger

static void config_info_color_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyInfoColor, tuple->value->int32);
}

static void config_background_color_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyBackgroundColor, tuple->value->int32);
  window_set_background_color(s_main_window, config_get_color(s_config, ConfigKeyBackgroundColor));
}

static void config_time_color_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyTimeColor, tuple->value->int32);
  layer_mark_dirty(s_tick_layer);

  text_block_mark_dirty(s_hour_text);
  text_block_mark_dirty(s_minute_text);
}

static void config_hour_hand_color_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyHourHandColor, tuple->value->int32);
  layer_mark_dirty(s_hour_hand_layer);
}

static void config_minute_hand_color_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyMinuteHandColor, tuple->value->int32);
  mark_dirty_minute_hand_layer();
}

static void config_refresh_rate_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyRefreshRate, tuple->value->int32);
}

static void config_temperature_unit_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyTemperatureUnit, tuple->value->int32);
  text_block_mark_dirty(s_weather_info);
}

#ifdef HIGH_DPI_INFO
static void config_distance_unit_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyDistanceUnit, tuple->value->int32);
  text_block_mark_dirty(s_steps_info);
}
#endif

static void config_bluetooth_icon_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyBluetoothIcon, tuple->value->int32);
  text_block_mark_dirty(s_watch_info);
}

static void config_battery_displayed_at_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_int(s_config, ConfigKeyBatteryDisplayedAt, tuple->value->int32);
  text_block_mark_dirty(s_watch_info);
}

#ifdef HIGH_DPI_INFO
static void config_extra_detail_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyExtraDetail, tuple->value->int8);
  // The blocks pick up (or drop) their second line on the redraw these mark.
  // Their collision boxes only shrink or grow once that redraw has happened, so
  // the quadrant layout catches up on the next minute tick.
  text_block_mark_dirty(s_date_info);
  text_block_mark_dirty(s_weather_info);
}
#endif

static void config_quiet_time_visible_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyQuietTimeVisible, tuple->value->int8);
  update_watch_info_layer_visibility();
  text_block_mark_dirty(s_watch_info);
}

static void config_date_displayed_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyDateDisplayed, tuple->value->int8);
  text_block_set_enabled(s_date_info, tuple->value->int8);
  text_block_mark_dirty(s_date_info);
}

static void config_rainbow_mode_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyRainbowMode, tuple->value->int8);
  mark_dirty_minute_hand_layer();
}

static void config_weather_enabled_updated(DictionaryIterator * iter, Tuple * tuple){
  const bool enabled = tuple->value->int8;
  config_set_bool(s_config, ConfigKeyWeatherEnabled, enabled);
  text_block_set_enabled(s_weather_info, enabled);
}

static void config_hourly_vibrate_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyVibrateOnTheHour, tuple->value->int8);
}

static void config_military_time_updated(DictionaryIterator * iter, Tuple * tuple){
  config_set_bool(s_config, ConfigKeyMilitaryTime, tuple->value->int8);
  text_block_mark_dirty(s_hour_text);
}

static void config_health_enabled_updated(DictionaryIterator * iter, Tuple * tuple){
  const bool enabled = tuple->value->int8;
  config_set_bool(s_config, ConfigKeyHealthEnabled, enabled);
  if(enabled){
    fetch_health(&s_context);
  }
#ifndef SCREENSHOT
  // In a SCREENSHOT build the steps block is force-enabled at load; don't let a
  // config push disable it.
  text_block_set_enabled(s_steps_info, enabled);
#endif
}

static void js_ready_callback(DictionaryIterator * iter, Tuple * tuple){
  s_js_ready = true;
  schedule_weather_request(NOW);
}

static void weather_requested_callback(DictionaryIterator * iter, Tuple * tuple){
  s_context.reset_weather = false;
  const Tuple * const icon_tuple = dict_find(iter, AppKeyWeatherIcon);
  const Tuple * const temp_tuple = dict_find(iter, AppKeyWeatherTemperature);
  if(icon_tuple && temp_tuple){
    s_context.weather.timestamp = time(NULL);
    s_context.weather.icon = icon_tuple->value->int8;
    s_context.weather.temperature = temp_tuple->value->int8;
    // The daily forecast is a separate Open-Meteo field and can be missing
    // (older phone app, trimmed response); the block then shows just the
    // current temperature, as before.
    const Tuple * const high_tuple = dict_find(iter, AppKeyWeatherHigh);
    const Tuple * const low_tuple = dict_find(iter, AppKeyWeatherLow);
    s_context.weather.has_range = high_tuple && low_tuple;
    if(s_context.weather.has_range){
      s_context.weather.high = high_tuple->value->int8;
      s_context.weather.low = low_tuple->value->int8;
    }
  }
  persist_write_data(PersistKeyWeather, &s_context.weather, sizeof(Weather));
  text_block_mark_dirty(s_weather_info);
  quadrants_update(s_quadrants, s_current_time);
}

static void messenger_callback(DictionaryIterator * iter){
  if(dict_find(iter, AppKeyConfig)){
    config_save(s_config, PersistKeyConfig);
    s_context.reset_weather = true;
    schedule_weather_request(NOW);
    quadrants_update(s_quadrants, s_current_time);
    layer_mark_dirty(s_root_layer);
  }
}

// Time

static bool times_conflicting(const tm * const time){
  return time->tm_hour % 12 == time->tm_min / 5;
}

// Position for the merged "H:MM" text when both times share a spoke. During a
// conflict both hands sit at or clockwise of the spoke (hour angle = 30h+m/2,
// minute angle = 6m, both in [30h, 30h+25) when h == m/5), so nudging the text
// counter-clockwise-perpendicular always clears them. The north/south spokes
// (11,0,1 and 5,6,7) need no nudge: the text sits radially beyond the hand
// tips there. The east/west spokes (2,3,4 and 8,9,10) also get pulled inward
// so the wide merged text doesn't clip the display edge.
static GPoint merged_time_point(const int index){
  GPoint p = time_points[index];
  if((index >= 2 && index <= 4) || (index >= 8 && index <= 10)){
    const int32_t angle = index * TRIG_MAX_ANGLE / 12;
    const int32_t sin_val = sin_lookup(angle);
    const int32_t cos_val = cos_lookup(angle);
    p.x += (int16_t)((-cos_val * TIME_CONFLICT_PERP - sin_val * TIME_CONFLICT_INSET) / TRIG_MAX_RATIO);
    p.y += (int16_t)((-sin_val * TIME_CONFLICT_PERP + cos_val * TIME_CONFLICT_INSET) / TRIG_MAX_RATIO);
  }
  return p;
}

// True when a text block centered here would reach into the strip covered by a
// system overlay. The overlay always grows up from the bottom of the screen.
static bool point_obstructed(const GPoint center){
  if(s_obstruction_height <= 0){
    return false;
  }
  const GRect frame = grect_from_center_and_size(center, TEXT_BLOCK_SIZE);
  return frame.origin.y + frame.size.h > s_root_layer_bounds.size.h - s_obstruction_height;
}

// Merge the hour and minute into a single "H:MM" when they share a spoke, or
// when either spoke has been swallowed by an overlay. The merged string states
// the time in full, so unlike a lone "6" it can be relocated somewhere visible
// without lying about where a hand is pointing.
static bool times_merged(const tm * const time){
  return times_conflicting(time)
    || point_obstructed(time_points[time->tm_hour % 12])
    || point_obstructed(time_points[time->tm_min / 5]);
}

// Spokes to relocate the merged time onto, nearest the top of the dial first.
// The bottom spokes (5, 6, 7) are omitted: they are the ones an overlay covers.
static const int merge_fallback_spokes[] = { 0, 11, 1, 10, 2, 9, 3 };

// Pick a visible spoke for the merged time that neither hand crosses. Falls
// back to the highest visible spoke if every candidate is crossed, since a hand
// drawn over the text still beats text hidden under the overlay.
static GPoint obstructed_merge_point(const tm * const time){
  const int hour_angle = angle_hour(time, true);
  const Segment hour_hand = SEGMENT(s_center, gpoint_on_circle(s_center, hour_angle, HOUR_HAND_RADIUS));
  const int minute_angle = angle_minute(time);
  const Segment minute_hand = SEGMENT(s_center, gpoint_on_circle(s_center, minute_angle, MINUTE_HAND_RADIUS));
  GPoint fallback = time_points[0];
  bool have_fallback = false;
  for(int i = 0; i < (int) ARRAY_LENGTH(merge_fallback_spokes); i++){
    const GPoint p = merged_time_point(merge_fallback_spokes[i]);
    if(point_obstructed(p)){
      continue;
    }
    if(!have_fallback){
      fallback = p;
      have_fallback = true;
    }
    const GRect frame = grect_from_center_and_size(p, TEXT_BLOCK_SIZE);
    if(!intersect(hour_hand, frame) && !intersect(minute_hand, frame)){
      return p;
    }
  }
  return fallback;
}

// Where the merged time goes: its own spoke when that is visible, otherwise the
// best spoke still clear of the overlay.
static GPoint merged_time_point_visible(const tm * const time){
  const GPoint natural = merged_time_point(time->tm_hour % 12);
  return point_obstructed(natural) ? obstructed_merge_point(time) : natural;
}

static void hour_time_update_proc(TextBlock * block){
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
  const GColor color = config_get_color(s_config, ConfigKeyTimeColor);
  char buffer[] = "00:00";
  const int hour = context->time->tm_hour;
  const int hour_mod_12 = hour % 12;
  const bool military_time = config_get_bool(config, ConfigKeyMilitaryTime);
  const int printed_hour = military_time ? hour : hour_mod_12 == 0 ? 12 : hour_mod_12;
  if(times_merged(context->time)){
    const int min = context->time->tm_min;
    snprintf(buffer, sizeof(buffer), "%d:%02d", printed_hour, min);
    text_block_set_text(block, buffer, color);
    text_block_move(block, merged_time_point_visible(context->time));
  }else{
    snprintf(buffer, sizeof(buffer), "%d", printed_hour);
    text_block_set_text(block, buffer, color);
    text_block_move(block, time_points[hour_mod_12]);
  }
}

static void minute_time_update_proc(TextBlock * block){
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
  const GColor color = config_get_color(config, ConfigKeyTimeColor);
  char buffer[] = "00";
  const int min = context->time->tm_min;
  if(times_merged(context->time)){
    text_block_set_text(s_minute_text, "", color);
  }else{
    text_block_set_enabled(s_minute_text, true);
    snprintf(buffer, sizeof(buffer), "%02d", min);
    text_block_move(s_minute_text, time_points[min / 5]);
    text_block_set_text(s_minute_text, buffer, color);
  }
}

// Date

static void date_info_update_proc(TextBlock * block){
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
  if(config_get_bool(config, ConfigKeyDateDisplayed)){
    const GColor date_color = config_get_color(config, ConfigKeyInfoColor);
    char buffer[] = "00";
    snprintf(buffer, sizeof(buffer), "%d", context->time->tm_mday);
    text_block_set_text(block, buffer, date_color);
#ifdef HIGH_DPI_INFO
    // Nupe has no alphabet (its letters are the weather/status icons), so the
    // weekday is the one piece of text on the face that has to come from a
    // system font.
    char weekday_buffer[8] = {0};
    if(config_get_bool(config, ConfigKeyExtraDetail)){
      strftime(weekday_buffer, sizeof(weekday_buffer), "%a", context->time);
      for(char * c = weekday_buffer; *c != '\0'; c++){
        if(*c >= 'a' && *c <= 'z'){
          *c -= 'a' - 'A';
        }
      }
    }
    // Always set it: an empty string is how the second line goes away again
    // when the setting is turned off.
    text_block_set_sub_text(block, weekday_buffer, date_color);
#endif
  }
}

// Hands
int s_animation_percent;

static void mark_dirty_minute_hand_layer(){
  layer_mark_dirty(s_minute_hand_layer);
  const bool rainbow_mode = config_get_bool(s_config, ConfigKeyRainbowMode);
  if(rainbow_mode){
    const float minute_angle = angle_minute(s_current_time);
    rot_bitmap_layer_set_angle(s_rainbow_hand_layer, minute_angle);
  }
  layer_set_hidden((Layer*)s_rainbow_hand_layer, !rainbow_mode);
}

static void update_minute_hand_layer(Layer *layer, GContext * ctx){
  if(!config_get_bool(s_config, ConfigKeyRainbowMode)){
    const float start_angle = angle(270, 360);
    const float minute_angle = angle_minute(s_current_time);
    const float hand_angle = minute_angle - start_angle * (100 - s_animation_percent) / 100;
    const GPoint hand_end = gpoint_on_circle(s_center, hand_angle, MINUTE_HAND_RADIUS);
    graphics_context_set_stroke_width(ctx, MINUTE_HAND_WIDTH);
    graphics_context_set_stroke_color(ctx, config_get_color(s_config, ConfigKeyMinuteHandColor));
    graphics_draw_line(ctx, s_center, hand_end);
  }
}


static void update_hour_hand_layer(Layer * layer, GContext * ctx){
  const float hour_angle = angle_hour(s_current_time, true);
  const float start_angle = angle(90, 360);
  const bool rainbow_mode = config_get_bool(s_config, ConfigKeyRainbowMode);
  const float hand_angle = rainbow_mode ? hour_angle : hour_angle - start_angle * (100 - s_animation_percent) / 100;
  const GPoint hand_end = gpoint_on_circle(s_center, hand_angle, HOUR_HAND_RADIUS);
  graphics_context_set_stroke_width(ctx, HOUR_HAND_WIDTH);
  graphics_context_set_stroke_color(ctx, config_get_color(s_config, ConfigKeyHourHandColor));
  graphics_draw_line(ctx, s_center, hand_end);
}

static void update_center_circle_layer(Layer * layer, GContext * ctx){
  const GColor color = config_get_bool(s_config, ConfigKeyRainbowMode) ? GColorVividViolet : config_get_color(s_config, ConfigKeyHourHandColor);
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, s_center, CENTER_CIRCLE_RADIUS);
}

// Ticks

static void draw_tick(GContext *ctx, const int index){
  graphics_draw_line(ctx, ticks_points[index][0], ticks_points[index][1]);
}

static void tick_layer_update_callback(Layer *layer, GContext *graphic_ctx) {
  const Context * const context = * (Context**) layer_get_data(layer);
  graphics_context_set_stroke_color(graphic_ctx, config_get_color(context->config, ConfigKeyTimeColor));
  graphics_context_set_stroke_width(graphic_ctx, TICK_WIDTH);
  const tm * const time = context->time;
  draw_tick(graphic_ctx, time->tm_hour % 12);
  if(times_conflicting(time)){
    return;
  }
  draw_tick(graphic_ctx, time->tm_min / 5);
}

// Weather

static int converted_temperature(const Config * const config, const int celsius){
  const bool is_farhrenheit = config_get_int(config, ConfigKeyTemperatureUnit) == Fahrenheit;
  return is_farhrenheit ? celsius * 9 / 5 + 32 : celsius;
}

static void weather_info_update_proc(TextBlock * block){
  char info_buffer[10] = {0};
#ifdef HIGH_DPI_INFO
  // Today's low and high, as "12° 24°". Nupe has no "/" glyph, a hyphen
  // separator is unreadable against negative temperatures ("-18--4"), and a
  // lone space is one digit wide, so "12 24" reads as one number. Carrying the
  // degree sign on both marks them as temperatures rather than any other pair.
  // Two degree signs are 2 bytes each in UTF-8: "-58° 122°" is 11 bytes plus
  // the terminator, so 16 leaves room for any pair the conversion can produce.
  char range_buffer[16] = {0};
  const bool extra_detail = config_get_bool(s_config, ConfigKeyExtraDetail);
#endif
#ifdef SCREENSHOT
  // Render mock weather (icon 'a', -12°) so the block appears in screenshots,
  // bypassing the received-weather validity/timeout check.
  snprintf(info_buffer, sizeof(info_buffer), "%c%d°", 'a', -12);
#ifdef HIGH_DPI_INFO
  if(extra_detail){
    snprintf(range_buffer, sizeof(range_buffer), "%d° %d°", -18, -4);
  }
#endif
#else
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
  const Weather weather = context->weather;
  const int timeout = (config_get_int(config, ConfigKeyRefreshRate) + 5) * 60;
  const int expiration =  weather.timestamp + timeout;
  const bool weather_valid = time(NULL) < expiration;
  if(weather_valid){
    snprintf(info_buffer, sizeof(info_buffer), "%c%d°", weather.icon, converted_temperature(config, weather.temperature));
#ifdef HIGH_DPI_INFO
    if(extra_detail && weather.has_range){
      snprintf(range_buffer, sizeof(range_buffer), "%d° %d°",
               converted_temperature(config, weather.low),
               converted_temperature(config, weather.high));
    }
#endif
  }
#endif
  const GColor info_color = config_get_color(s_config, ConfigKeyInfoColor);
  text_block_set_text(block, info_buffer, info_color);
#ifdef HIGH_DPI_INFO
  // Empty when the setting is off or the forecast is missing, which is how the
  // second line disappears again.
  text_block_set_sub_text(block, range_buffer, info_color);
#endif
}

static void send_weather_request_callback(void * context){
  s_weather_request_timer = NULL;
  const int timeout = config_get_int(s_config, ConfigKeyRefreshRate) * 60;
  const int expiration =  s_context.weather.timestamp + timeout;
  const bool almost_expired = time(NULL) > expiration;
  const bool can_update_weather = (s_context.reset_weather || almost_expired) && s_js_ready;
  if(can_update_weather){
    if(config_get_bool(s_config, ConfigKeyWeatherEnabled)){
      DictionaryIterator *out_iter;
      AppMessageResult result = app_message_outbox_begin(&out_iter);
      if(result == APP_MSG_OK) {
        const int value = 1;
        dict_write_int(out_iter, AppKeyWeatherRequest, &value, sizeof(int), true);
        result = app_message_outbox_send();
        if(result != APP_MSG_OK) {
          schedule_weather_request(5000);
        }
      } else {
        schedule_weather_request(5000);
      }
    }
  }
}

static void schedule_weather_request(const int timeout){
  const int expiration = time(NULL) + timeout;
  if(s_weather_request_timer){
    if(expiration < s_weather_request_timeout){
      s_weather_request_timeout = expiration;
      app_timer_reschedule(s_weather_request_timer, timeout);
    }
  }else{
    s_weather_request_timeout = expiration;
    s_weather_request_timer = app_timer_register(timeout, send_weather_request_callback, NULL);
  }
}

// Battery + Bluetooth + Quiet Time

static void watch_info_update_proc(TextBlock * block){
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
  char info_buffer[4] = {0};
  const BluetoothIcon bluetooth_icon = config_get_int(config, ConfigKeyBluetoothIcon);
#ifdef SCREENSHOT
  const bool bluetooth_disconneted = true;  // force the BT glyph on for screenshots
#else
  const bool bluetooth_disconneted = !context->bluetooth_connected;
#endif
  const bool bluetooth_icon_set = bluetooth_icon != NoIcon;
  if(bluetooth_disconneted && bluetooth_icon_set){
    strncat(info_buffer, bluetooth_icon == Bluetooth ? "z" : "Z", 2);
  }
#ifdef SCREENSHOT
  const bool battery_below_threshold = true;  // force the battery glyph on for screenshots
#else
  const int battery_threshold = config_get_int(config, ConfigKeyBatteryDisplayedAt);
  const BatteryChargeState charge_state = context->charge_state;
  const bool battery_below_threshold = charge_state.charge_percent < battery_threshold;
#endif
  if(battery_below_threshold){
    strncat(info_buffer, "w", 2);
  }
#ifdef SCREENSHOT
  const bool quiet_time_visible = true;  // force the quiet-time glyph on for screenshots
#else
  const bool quiet_time_visible = quiet_time_is_active() && config_get_bool(config, ConfigKeyQuietTimeVisible);
#endif
  if(quiet_time_visible){
    strncat(info_buffer, "q", 2);
  }
  const GColor info_color = config_get_color(s_config, ConfigKeyInfoColor);
  text_block_set_text(block, info_buffer, info_color);
}

// Steps

static void steps_info_update_proc(TextBlock * block){
  const Context * const context = (Context *) text_block_get_context(block);
  const Config * const config = context->config;
#ifdef SCREENSHOT
  const int steps = 6234;  // mock step count renders as "y6.2k" for screenshots
#else
  const int steps = context->steps;
#endif
  char step_text[16] = {0};
  const GColor info_color = config_get_color(config, ConfigKeyInfoColor);
  if(steps > 10000){
    snprintf(step_text, sizeof(step_text), "y%dk", steps / 1000);
  }else if(steps > 1000){
    snprintf(step_text, sizeof(step_text), "y%d.%dk", steps / 1000, (steps % 1000) / 100);
  }else{
    snprintf(step_text, sizeof(step_text), "y%d", steps);
  }
  text_block_set_text(block, step_text, info_color);
#ifdef HIGH_DPI_INFO
  // Distance walked today, under the step count. "km" and "mi" are letters, so
  // like the weekday this line is system Gothic rather than Nupe.
  char distance_text[12] = {0};
  if(config_get_bool(config, ConfigKeyExtraDetail)){
#ifdef SCREENSHOT
    const int meters = 4823;  // mock distance renders as "4.8 km"
#else
    const int meters = context->distance_meters;
#endif
    const bool in_miles = config_get_int(config, ConfigKeyDistanceUnit) == Miles;
    // Tenths in integer math, rounded rather than truncated: half a unit is
    // added before the divide, so 4823m reads 3.0 mi and not 2.9. Distances are
    // never negative, so the rounding needs no sign handling. Using 1609 rather
    // than 1609.344 costs a tenth only past 700 miles walked in one day.
    const int tenths = in_miles ? (meters * 10 + 804) / 1609 : (meters + 50) / 100;
    snprintf(distance_text, sizeof(distance_text), "%d.%d %s", tenths / 10, tenths % 10, in_miles ? "mi" : "km");
  }
  text_block_set_sub_text(block, distance_text, info_color);
#endif
}

static void fetch_health(Context * const context){
  if(config_get_bool(context->config, ConfigKeyHealthEnabled)){
    context->steps = (int)health_service_sum_today(HealthMetricStepCount);
    context->distance_meters = (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
  }
}

// Event handlers

static void update_watch_info_layer_visibility(){
#ifdef SCREENSHOT
  // Keep the watch info block visible for screenshots regardless of BT/battery
  // state (this runs at load and from the BT/battery handlers).
  text_block_set_enabled(s_watch_info, true);
#else
  const Config * const config = s_context.config;
  const bool battery_icon_visible = s_context.charge_state.charge_percent < config_get_int(config, ConfigKeyBatteryDisplayedAt);
  const bool bt_icon_visible = !s_context.bluetooth_connected && config_get_int(config, ConfigKeyBluetoothIcon) != NoIcon;
  const bool quiet_icon_visible = quiet_time_is_active() && config_get_bool(config, ConfigKeyQuietTimeVisible);
  text_block_set_enabled(s_watch_info, battery_icon_visible || bt_icon_visible || quiet_icon_visible);
#endif
}

static void bt_handler(bool connected){
  if(connected){
    schedule_weather_request(NOW);
  }
  s_context.bluetooth_connected = connected;
  update_watch_info_layer_visibility();
  text_block_mark_dirty(s_watch_info);
}

static void battery_handler(BatteryChargeState charge){
  s_context.charge_state = charge;
  update_watch_info_layer_visibility();
  text_block_mark_dirty(s_watch_info);
}

static void step_handler(HealthEventType event, void * context){
  if(event == HealthEventSignificantUpdate){
    fetch_health((Context *)context);
    text_block_mark_dirty(s_steps_info);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed){
  if(HOUR_UNIT & units_changed){
    bool vibrate_on_the_hour = config_get_bool(s_config, ConfigKeyVibrateOnTheHour);
    if (vibrate_on_the_hour) {
      if( !PBL_IF_HEALTH_ELSE(config_get_bool(s_config, ConfigKeyHealthEnabled), false)  ||
        !(health_service_peek_current_activities() &
          (HealthActivitySleep | HealthActivityRestfulSleep)) ){
        vibes_short_pulse();
      }
    }
  }
  schedule_weather_request(10000);
  update_current_time();
  fetch_health(&s_context);

  layer_mark_dirty(s_hour_hand_layer);
  layer_mark_dirty(s_tick_layer);
  mark_dirty_minute_hand_layer();

  text_block_mark_dirty(s_hour_text);
  text_block_mark_dirty(s_minute_text);
  text_block_mark_dirty(s_date_info);
  text_block_mark_dirty(s_steps_info);

  // Quiet Time has no event to subscribe to; refresh its icon on the minute.
  update_watch_info_layer_visibility();
  text_block_mark_dirty(s_watch_info);

  quadrants_update(s_quadrants, s_current_time);
}

static void implementation_update(Animation *animation,
                                  const AnimationProgress progress) {
  s_animation_percent = ((int)progress * 100) / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(s_hour_hand_layer);
  layer_mark_dirty(s_minute_hand_layer);
}

const AnimationImplementation implementation = {
  .update = implementation_update,
};

// Unobstructed area (Timeline Quick View). Aplite stubs the whole service out
// to a no-op, so the handlers would be dead code there.
// This is what PBL_API_EXISTS() expands to; spelled out because the macro hides
// a `defined` inside an expansion, which -Wexpansion-to-defined flags.
#if defined(_PBL_API_EXISTS_unobstructed_area_service_subscribe)
#define HAS_UNOBSTRUCTED_AREA 1

static void refresh_obstruction(const int obstruction_height){
  if(obstruction_height == s_obstruction_height){
    return;
  }
  s_obstruction_height = obstruction_height;
  text_block_mark_dirty(s_hour_text);
  text_block_mark_dirty(s_minute_text);
}

// Move the time before the overlay slides in rather than after, so it is never
// briefly buried mid-animation.
static void unobstructed_will_change(GRect final_unobstructed_screen_area, void * context){
  refresh_obstruction(s_root_layer_bounds.size.h - final_unobstructed_screen_area.size.h);
}

static void unobstructed_did_change(void * context){
  refresh_obstruction(s_root_layer_bounds.size.h - layer_get_unobstructed_bounds(s_root_layer).size.h);
}
#endif

static void main_window_load(Window *window) {
  s_root_layer = window_get_root_layer(window);
  s_root_layer_bounds = layer_get_bounds(s_root_layer);
  s_obstruction_height = s_root_layer_bounds.size.h - layer_get_unobstructed_bounds(s_root_layer).size.h;
  s_center = grect_center_point(&s_root_layer_bounds);
  update_current_time();
  window_set_background_color(window, config_get_color(s_config, ConfigKeyBackgroundColor));

  s_quadrants = quadrants_create(s_center, HOUR_HAND_RADIUS, MINUTE_HAND_RADIUS);
  s_date_info = quadrants_add_text_block(s_quadrants, s_root_layer, s_font, Low, s_current_time);
#ifdef HIGH_DPI_INFO
  text_block_set_sub_font(s_date_info, fonts_get_system_font(FONT_KEY_GOTHIC_14), SUB_TEXT_HEIGHT_GOTHIC_14, SUB_TEXT_PULL_GOTHIC_14_ABOVE, true);
#endif
  text_block_set_enabled(s_date_info, config_get_bool(s_config, ConfigKeyDateDisplayed));
  text_block_set_context(s_date_info, &s_context);
  text_block_set_update_proc(s_date_info, date_info_update_proc);

  s_steps_info = quadrants_add_text_block(s_quadrants, s_root_layer, s_font, High, s_current_time);
#ifdef SCREENSHOT
  text_block_set_enabled(s_steps_info, true);  // force the steps block on for screenshots
#else
  text_block_set_enabled(s_steps_info, config_get_bool(s_config, ConfigKeyHealthEnabled));
#endif
#ifdef HIGH_DPI_INFO
  text_block_set_sub_font(s_steps_info, fonts_get_system_font(FONT_KEY_GOTHIC_14), SUB_TEXT_HEIGHT_GOTHIC_14, SUB_TEXT_PULL_GOTHIC_14_BELOW, false);
#endif
  text_block_set_context(s_steps_info, &s_context);
  text_block_set_update_proc(s_steps_info, steps_info_update_proc);
  health_service_events_subscribe(step_handler, &s_context);
  fetch_health(&s_context);

  s_weather_info = quadrants_add_text_block(s_quadrants, s_root_layer, s_font, Head, s_current_time);
#ifdef HIGH_DPI_INFO
  text_block_set_sub_font(s_weather_info, s_sub_font, SUB_TEXT_HEIGHT_NUPE_18, SUB_TEXT_PULL_NUPE_18_BELOW, false);
#endif
  text_block_set_enabled(s_weather_info, config_get_bool(s_config, ConfigKeyWeatherEnabled));
  text_block_mark_dirty(s_weather_info);
  text_block_set_context(s_weather_info, &s_context);
  text_block_set_update_proc(s_weather_info, weather_info_update_proc);

  s_watch_info = quadrants_add_text_block(s_quadrants, s_root_layer, s_font, Tail, s_current_time);
  text_block_set_context(s_watch_info, &s_context);
  text_block_set_update_proc(s_watch_info, watch_info_update_proc);
  bluetooth_connection_service_subscribe(bt_handler);
  bt_handler(connection_service_peek_pebble_app_connection());
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());
  update_watch_info_layer_visibility();

  s_hour_text = text_block_create(s_root_layer, time_points[6] , s_font);
  text_block_set_context(s_hour_text, &s_context);
  text_block_set_update_proc(s_hour_text, hour_time_update_proc);

  s_minute_text = text_block_create(s_root_layer, time_points[0] , s_font);
  text_block_set_context(s_minute_text, &s_context);
  text_block_set_update_proc(s_minute_text, minute_time_update_proc);

  s_tick_layer = layer_create_with_data(s_root_layer_bounds, sizeof(Context*));
  Context ** data = (Context**) layer_get_data(s_tick_layer);
  *data = &s_context;
  layer_set_update_proc(s_tick_layer, tick_layer_update_callback);
  layer_add_child(s_root_layer, s_tick_layer);

  s_rainbow_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMG_RAINBOW_HAND);
  s_minute_hand_layer   = layer_create(s_root_layer_bounds);
  s_hour_hand_layer     = layer_create(s_root_layer_bounds);
  s_center_circle_layer = layer_create(s_root_layer_bounds);
  s_rainbow_hand_layer  = rot_bitmap_layer_create(s_rainbow_bitmap);
  rot_bitmap_set_compositing_mode(s_rainbow_hand_layer, GCompOpSet);
  const GPoint png_center = GPoint(RAINBOW_HAND_OFFSET_X, RAINBOW_HAND_OFFSET_Y);
  rot_bitmap_set_src_ic(s_rainbow_hand_layer, png_center);
  GRect frame = layer_get_frame((Layer *) s_rainbow_hand_layer);
  frame.origin.x = s_center.x - frame.size.w / 2;
  frame.origin.y = s_center.y - frame.size.h / 2;
  layer_set_frame((Layer *)s_rainbow_hand_layer, frame);
  layer_set_update_proc(s_hour_hand_layer,     update_hour_hand_layer);
  layer_set_update_proc(s_minute_hand_layer,   update_minute_hand_layer);
  layer_set_update_proc(s_center_circle_layer, update_center_circle_layer);
  layer_add_child(s_root_layer, s_minute_hand_layer);
  layer_add_child(s_root_layer, (Layer *)s_rainbow_hand_layer);
  layer_add_child(s_root_layer, s_hour_hand_layer);
  layer_add_child(s_root_layer, s_center_circle_layer);
  mark_dirty_minute_hand_layer();

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
#ifdef HAS_UNOBSTRUCTED_AREA
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) {
    .will_change = unobstructed_will_change,
    .did_change = unobstructed_did_change
  }, NULL);
#endif

  quadrants_update(s_quadrants, s_current_time);

  Animation *animation = animation_create();
  animation_set_curve(animation, AnimationCurveEaseInOut);

  animation_set_delay(animation, 0);
  animation_set_duration(animation, 1000);

  animation_set_implementation(animation, &implementation);

  animation_schedule(animation);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_hour_hand_layer);
  rot_bitmap_layer_destroy(s_rainbow_hand_layer);
  layer_destroy(s_minute_hand_layer);
  layer_destroy(s_center_circle_layer);
  gbitmap_destroy(s_rainbow_bitmap);

  text_block_destroy(s_hour_text);
  text_block_destroy(s_minute_text);

  layer_destroy(s_tick_layer);

  if(config_get_bool(s_config, ConfigKeyHealthEnabled)){
    health_service_events_unsubscribe();
  }
  bluetooth_connection_service_unsubscribe();
  unobstructed_area_service_unsubscribe();

  s_quadrants = quadrants_destroy(s_quadrants);

  text_block_destroy(s_weather_info);
  text_block_destroy(s_date_info);
  text_block_destroy(s_steps_info);
  text_block_destroy(s_watch_info);
}

static void init() {
  Message messages[] = {
    { AppKeyJsReady, js_ready_callback },
    { AppKeyBackgroundColor, config_background_color_updated },
    { AppKeyHourHandColor, config_hour_hand_color_updated },
    { AppKeyInfoColor, config_info_color_updated },
    { AppKeyMinuteHandColor, config_minute_hand_color_updated },
    { AppKeyTimeColor, config_time_color_updated },
    { AppKeyDateDisplayed, config_date_displayed_updated },
    { AppKeyRainbowMode, config_rainbow_mode_updated },
    { AppKeyBluetoothIcon, config_bluetooth_icon_updated },
    { AppKeyQuietTimeVisible, config_quiet_time_visible_updated },
    { AppKeyRefreshRate, config_refresh_rate_updated },
    { AppKeyTemperatureUnit, config_temperature_unit_updated },
    { AppKeyWeatherEnabled, config_weather_enabled_updated },
    { AppKeyWeatherTemperature, weather_requested_callback },
    { AppKeyVibrateOnTheHour, config_hourly_vibrate_updated },
    { AppKeyMilitaryTime, config_military_time_updated },
    { AppKeyHealthEnabled, config_health_enabled_updated },
    { AppKeyBatteryDisplayedAt, config_battery_displayed_at_updated },
#ifdef HIGH_DPI_INFO
    { AppKeyExtraDetail, config_extra_detail_updated },
    { AppKeyDistanceUnit, config_distance_unit_updated }
#endif
  };
  s_messenger = messenger_create(ARRAY_LENGTH(messages), messenger_callback, messages);
  s_weather_request_timeout = 0;
  s_js_ready = false;
#if defined(PBL_PLATFORM_GABBRO)
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_NUPE_33));
#elif defined(PBL_PLATFORM_EMERY)
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_NUPE_28));
#else
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_NUPE_23));
#endif
#ifdef HIGH_DPI_INFO
  s_sub_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_NUPE_18));
#endif
  s_config = config_load(PersistKeyConfig, CONF_SIZE, CONF_DEFAULTS);
  s_context = (Context) {
    .config = s_config,
    .steps = 0,
    .reset_weather = false,
    .bluetooth_connected = false,
    .charge_state = (BatteryChargeState) {
      .charge_percent = 100,
      .is_charging = false,
      .is_plugged = false
    },
    .time = NULL
  };
  if(persist_exists(PersistKeyWeather)){
    persist_read_data(PersistKeyWeather, &s_context.weather, sizeof(Weather));
  }
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_stack_remove(s_main_window, true);
  window_destroy(s_main_window);
  s_config = config_destroy(s_config);
  fonts_unload_custom_font(s_font);
#ifdef HIGH_DPI_INFO
  fonts_unload_custom_font(s_sub_font);
#endif
  s_messenger = messenger_destroy(s_messenger);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
