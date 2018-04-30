#include <pebble.h>

static Window *s_window;
static TextLayer *s_text_layer;
static TextLayer *s_min_left_layer;
static TextLayer *s_next_time_layer;
static Layer *watch_layer;


// die Vorlesungsbloecke
static int *times = (int[])
  { 8*60+30, 10*60+0,
    10*60+15, 11*60+45,
    12*60+0, 13*60+30,
    14*60+15, 15*60+45,
    16*60+0, 17*60+30,
    17*60+45, 19*60+15
};

// die Vorlesungsbloecke
static int *timesMonday = (int[])
  { 8*60+30, 10*60+0,
    10*60+15, 11*60+45,
    12*60+30, 14*60+0,
    14*60+15, 15*60+45,
    16*60+0, 17*60+30,
    17*60+45, 19*60+15
};




// progress goes from 0 to 89 (the minutes of a "vorlesung")
static void drawProgressLine(GContext * ctx,int cx,int cy,int progress,int innerRadius,int outerRadius) {


    float sin = sin_lookup(TRIG_MAX_ANGLE * progress / 90) / (float) TRIG_MAX_RATIO;
    float cos = cos_lookup(TRIG_MAX_ANGLE * progress / 90) / (float) TRIG_MAX_RATIO;


    // Find the start points
    int x1 = cx + sin * innerRadius;
    int y1 = cy - cos * innerRadius;

    // find the end points
    int x2 = cx + sin * outerRadius;
    int y2 = cy - cos * outerRadius;


    GPoint start = GPoint(x1, y1);
    GPoint end = GPoint(x2, y2);

    // Draw a line
    graphics_draw_line(ctx, start, end);


}



// vergleicht die uebergebene summe Minute mit den Werten in den Vorlesungsbloecken.
// entweder man ist vor einem block (dann wird -index zurueckgegeben)
// oder man ist in einem Block, dann wird index zurueckgegeben
// nach dem letzten block wird -100 (d.h. komplett ausserhalb zurueckgegeben)
int checkRange(int summeMinutes, bool isMonday) {
    int *actualTimes = times;
    if (isMonday) {
        actualTimes = timesMonday;
    }
    for (int index = 0;index < 12;index += 2) {

        if (summeMinutes < *(actualTimes+index)) {

            return -index-1;
        }
        if (summeMinutes < *(actualTimes+index+1)) {
            return index;
        }
    }
    return -100;
}

/* calculates the minutes that are green (already passed) based on the current time */
static int calcMinutesGreen(struct tm *tick_time) {

    int summeMinuten = tick_time->tm_hour * 60 + tick_time->tm_min;

    bool isMonday = tick_time->tm_wday == 1;

    int *actualTimes = times;
    if (isMonday) {
	actualTimes = timesMonday;
    }
    int rangeIndex = checkRange(summeMinuten,isMonday);
    if ( rangeIndex >= 0) {

        // we are within a block, the starttime is before the current summeMinuten Value

        int startTime = *(actualTimes + rangeIndex);

        int minutesGreen = summeMinuten - startTime;

        return minutesGreen;

    } else {


        if (rangeIndex > -100) {
            // it is an inverted index to the next range ...
            int startTime = *(actualTimes - rangeIndex-1);
            // startTime is after the current summeMinuten Value
            return summeMinuten - startTime; // then this will be the negative starting time
        }
        // we don't have a following block
        return -1000;
    }

}

static char* next_time_str = "1234567";

static void calcNextTimeString(struct tm * tick_time) {

    int summeMinuten = tick_time->tm_hour * 60 + tick_time->tm_min;

    bool isMonday = tick_time->tm_wday==1;
    int * actualTimes = times;
    if (isMonday) {
	actualTimes = timesMonday;
    }
    int rangeIndex = checkRange(summeMinuten, isMonday);
    if ( rangeIndex >= 0) {

        int endTime = *(actualTimes + rangeIndex + 1);

        int hours = endTime / 60;
        int minutes = (endTime - (hours * 60));

        snprintf(next_time_str,6,"%02d:%02d",hours,minutes);
        return;

    } else {
        if (rangeIndex > -100) {
            // it is an inverted index to the next range ...
            int startTime = *(actualTimes - rangeIndex - 1);


            int hours = startTime / 60;
            int minutes = (startTime - (hours * 60));

            snprintf(next_time_str,6,"%02d:%02d",hours,minutes);

            return;
        }
    }


      strftime(next_time_str,7, "%d %b", tick_time);

}


static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!

    // Set the stroke width (must be an odd integer value)
    graphics_context_set_stroke_width(ctx, 3);

    // Disable antialiasing (enabled by default where available)
    graphics_context_set_antialiased(ctx, true);

    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer), "%H:%M", tick_time);
    //snprintf(s_buffer,7,"%d",tick_time->tm_wday); 
    text_layer_set_text(s_text_layer,s_buffer);

    calcNextTimeString(tick_time);
    text_layer_set_text(s_next_time_layer,next_time_str);

    int minutesGreen = calcMinutesGreen(tick_time);

    int cx = 90;
    int cy = 90;

    if (minutesGreen < 0) {

        if ( minutesGreen > -100) {
            static char s_buffer2[12];
            snprintf(s_buffer2,sizeof(s_buffer2),"%02d until",-minutesGreen);
            text_layer_set_text_color(s_min_left_layer,GColorRed);
            text_layer_set_text(s_min_left_layer,s_buffer2);


        } else {
            // we do not have a following block ....
            static char s_buffer2[13];
            snprintf(s_buffer2,sizeof(s_buffer2),"done for now");
            text_layer_set_text_color(s_min_left_layer,GColorBlue);
            text_layer_set_text(s_min_left_layer,s_buffer2);

        }
    } else {

        static char s_buffer2[12];
        text_layer_set_text_color(s_min_left_layer,GColorBlack);

        snprintf(s_buffer2,sizeof(s_buffer2),"%02d min left",(90 - minutesGreen));
        text_layer_set_text(s_min_left_layer,s_buffer2);

        int maxLength = (180 - 18) / 2;

        graphics_context_set_stroke_color(ctx, GColorDarkGreen);

        for (int i = 0; i < 90 && i < minutesGreen; i++) {
            drawProgressLine(ctx, cx, cy, i, maxLength - 10, maxLength);
        }
   }

}


static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  window_set_background_color(window,GColorWhite);

  watch_layer = layer_create(bounds);

  s_text_layer = text_layer_create(GRect(0, 80, bounds.size.w, 44));
  text_layer_set_text(s_text_layer, "42:42");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_text_layer, GColorClear);
  text_layer_set_text_color(s_text_layer,GColorBlack);
  text_layer_set_font(s_text_layer,fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  layer_add_child(watch_layer, text_layer_get_layer(s_text_layer));


  s_min_left_layer = text_layer_create(GRect(0, 30, bounds.size.w, 28));
  text_layer_set_text(s_min_left_layer, "empty1");
  text_layer_set_text_alignment(s_min_left_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_min_left_layer, GColorClear);
  text_layer_set_text_color(s_min_left_layer,GColorBlack);
  text_layer_set_font(s_min_left_layer,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(watch_layer, text_layer_get_layer(s_min_left_layer));


  s_next_time_layer = text_layer_create(GRect(0, 58, bounds.size.w, 28));
  text_layer_set_text(s_next_time_layer, "empty2");
  text_layer_set_text_alignment(s_next_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_next_time_layer, GColorClear);
  text_layer_set_text_color(s_next_time_layer,GColorBlack);
  text_layer_set_font(s_next_time_layer,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(watch_layer, text_layer_get_layer(s_next_time_layer));


  layer_set_update_proc(watch_layer, canvas_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), watch_layer);

}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
  text_layer_destroy(s_min_left_layer);
  text_layer_destroy(s_next_time_layer);
  layer_destroy(watch_layer);
}


static void tick_handler_minutes(struct tm *tick_time, TimeUnits units_changed) {

    layer_mark_dirty(watch_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler_minutes);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}





/*
// Redraw the screen
rocky.on('draw', function(event) {
  // Drawing canvas
  var ctx = event.context;

    // Determine the available width and height of the display
    var w = ctx.canvas.unobstructedWidth;
    var h = ctx.canvas.unobstructedHeight;

    // Determine the center point of the display
    var cx = w / 2;
    var cy = h / 2;


  // Clear the canvas
  // https://developer.pebble.com/docs/rockyjs/CanvasRenderingContext2D/#Canvas
  ctx.clearRect(0, 0, ctx.canvas.clientWidth, ctx.canvas.clientHeight);

  // UnobstructedArea
  // https://developer.pebble.com/docs/rockyjs/CanvasRenderingContext2D/#Canvas
  var offsetY = (ctx.canvas.clientHeight - ctx.canvas.unobstructedHeight) / 2;
  var centerX = ctx.canvas.unobstructedWidth / 2;

  // Text formatting
  ctx.fillStyle = 'white';
  ctx.textAlign = 'center';

  // Time font
  // https://developer.pebble.com/docs/rockyjs/CanvasRenderingContext2D/#font
  ctx.font = '26px bold Leco-numbers-am-pm';

  // Time
  ctx.fillText(clockData.time, centerX, (66 - offsetY));

  // Date font
  ctx.font = '18px bold Gothic';


    if (clockData.minutesGreen <= 0) {
        // Date
        ctx.fillText(clockData.date, centerX, (94 - offsetY));

    } else {

        ctx.fillText( (90-clockData.minutesGreen) + ' min left', centerX, (94 - offsetY));

        var maxLength = (Math.min(w, h) - 18) / 2;

        for (var i = 0; i < 90; i++) {
            var color = "green";
            if (i > clockData.minutesGreen) {
                color = "gray";
            }
            drawProgressLine(ctx, cx, cy, i, maxLength - 8, maxLength, color);
        }
    }



//    var d = new Date();

    // -20 so we're inset 10px on each side
//    var maxLength = (Math.min(w, h) - 20) / 2;

    // Calculate the minute hand angle
//    var minuteFraction = (d.getMinutes()) / 60;
//    var minuteAngle = fractionToRadian(minuteFraction);

    // Draw the minute hand
//    drawHand(ctx, cx, cy, minuteAngle, maxLength, "white");



});
*/

// Send a single message to the Phone
// https://developer.pebble.com/docs/rockyjs/rocky/#postMessage
// rocky.postMessage("This arrives on the phone via bluetooth!");



int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
