/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */
#include <lvgl.h>
#include <FastEPD.h>
FASTEPD epaper;
lv_obj_t *label, *msg_label;
lv_obj_t *btn1, *btn2, *btn3;
lv_obj_t *label_a, *label_b, *label_c;
const lv_point_t points_array[3] = { {250,110}, {640,110}, {1130, 110}};

#define BUTTON_A 3
#define BUTTON_B 46
#define BUTTON_C 38

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 *Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 *as the examples and demos are now part of the main LVGL library. */

//#include <examples/lv_examples.h>
//#include <demos/lv_demos.h>

#define DRAW_BUF_SIZE(w, h) (((w * h) / 10 * LV_COLOR_DEPTH)/8)
uint16_t *draw_buf;

#if LV_USE_LOG != 0
void my_print( lv_log_level_t level, const char * buf )
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Copy `px map` to the `area`*/
    static int iUpdates = 0; // count eink updates to know when to do a fullUpdate()
    static int iTop = 2000, iBottom = 0; // keep track of the range of rows to speed up partial updates
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    int i, iPitch;
    uint8_t uc, count, *s, *d; // we only operate in LVGL's A8 (8-bit grayscale) mode
    uint16_t u16, *s16;
    uint8_t u8Gray;

    s = px_map;
    // LVGL now supports a 1-bit graphics mode
    if (area->y1 < iTop) iTop = area->y1;
    if (area->y2 > iBottom) iBottom = area->y2;
#if LV_COLOR_DEPTH == 1
    if (epaper.getMode() == BB_MODE_1BPP) {
        int iSrcPitch = (w+7)/8;
        iPitch = (epaper.width() + 7) / 8;
        d = epaper.currentBuffer();
        d += (area->y1 * iPitch) + (area->x1 / 8);
        for (int y=0; y<h; y++) {
            memcpy(d, s, iSrcPitch);
            d += iPitch;
            s += iSrcPitch;
        }
    } else { // 4bpp dest
        int iSrcPitch = (w+7)/8;
        iPitch = epaper.width()/2;
        d = epaper.currentBuffer();
        d += (area->y1 * iPitch) + (area->x1 / 2);
        for (int y=0; y<h; y++) {
            memcpy(d, s, iSrcPitch);
            d += iPitch;
            s += iSrcPitch;
        }
    }
#elif LV_COLOR_DEPTH == 8
    if (epaper.getMode() == BB_MODE_1BPP) {
        iPitch = (epaper.width() + 7)/8;
        for (int y=0; y<h; y++) {
            d = epaper.currentBuffer();
            d += ((y+area->y1) * iPitch) + (area->x1 >> 3);
            s = &px_map[y * w];
            i = area->x1 & 7;
            if (i == 0) { // starting on byte boundary 
                count = 8;
                uc = 0;
            } else {
                count = 8 - i;
                uc = d[0] >> count; // get partial byte being overwritten
            }
            for (int x=0; x<w; x++) {
              uc <<= 1;
              uc |= (s[x] >= 128); // threshold in the middle of the range
              count--;
              if (count == 0) {
                *d++ = uc;
                uc = 0;
                count = 8;
              }
            } // for x
            if (count != 8) {
                uint8_t uc2 = d[0];
                uc2 &= (0xff >> (8-count));
                uc2 |= (uc << count); 
                *d++ = uc2; // store partial byte
            }
        } // for y
    } else { // 4-bpp
        for (int y=0; y<h; y++) {
            d = epaper.currentBuffer();
            d += ((y+area->y1) * iPitch) + (area->x1 >> 1);
            s = &px_map[y * w];
            for (int x=0; x<w; x+=2) { // work in pairs of pixels
                uc = (s[x] & 0xf0) | (s[x+1] >> 4);
                *d++ = uc;
            } // for x
        } // for y
    }
#else // Must be RGB565
    if (epaper.getMode() == BB_MODE_1BPP) {
        iPitch = (epaper.width() + 7) / 8;
        for (int y=0; y<h; y++) {
            s16 = (uint16_t *)&px_map[y * w * 2];
            d = epaper.currentBuffer();
            d += ((y+area->y1) * iPitch) + (area->x1 >> 3);
            if (area->x1 & 7 == 0) {
                count = 8;
                uc = 0;
            } else {
                count = 8; // DEBUG
                uc = 0;
            }
            for (int x=0; x<w; x++) {
              uc <<= 1;
              u16 = *s16++;
              u8Gray = (u16 & 0x1f) + ((u16 & 0x7e0) >> 5) + (u16 >> 11);
              uc |= (u8Gray >= 64); // threshold in the middle of the range
              count--;
              if (count == 0) {
                *d++ = uc;
                uc = 0;
                count = 8;
              }
            } // for x
        } // for y
    } else { // 4-bpp
        iPitch = (epaper.width() + 1) / 2;
        for (int y=0; y<h; y++) {
            d = epaper.currentBuffer();
            d += ((y+area->y1) * iPitch) + (area->x1 >> 1);
            s16 = (uint16_t *)&px_map[y * w * 2];
            for (int x=0; x<w; x+=2) { // work in pairs of pixels
                u16 = *s16++;
                u8Gray = (u16 & 0x1f) + ((u16 & 0x7e0) >> 5) + (u16 >> 11);
                u8Gray >>= 3; // turn it into a 4-bit grayscale value 
                uc = u8Gray << 4; // left of the pair (top 4 bits)
                u16 = *s16++;
                u8Gray = (u16 & 0x1f) + ((u16 & 0x7e0) >> 5) + (u16 >> 11);
                uc |= (u8Gray >> 3);
                *d++ = uc;
            } // for x
        } // for y
    } // 4-bpp
#endif
    /*tell LVGL you are ready*/
  //  Serial.printf("area update: %d,%d to %d,%d, w=%d\n", area->x1, area->y1, area->x2, area->y2, w);
    lv_display_flush_ready(disp);
    if (lv_disp_flush_is_last(disp)) { // time to update the eink
        if (epaper.getMode() == BB_MODE_1BPP && iUpdates < 50) {
            epaper.partialUpdate(true); //, iTop, iBottom);
            iTop = 2000; iBottom = 0; // reset top/bottom row checks
        } else { // 16-gray mode only supports full updates, or after 50 partial updates
            epaper.fullUpdate(false, false);
            iUpdates = 0;
        }
        iUpdates++;
    }
}

/* Read the button states */
void my_button_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    static uint32_t last_btn = 0;   /* Store the last pressed button */
    int btn_pr = -1;     /* Get the ID (0,1,2...) of the pressed button */
    if (digitalRead(BUTTON_A) == 0) btn_pr = 0;
    else if (digitalRead(BUTTON_B) == 0) btn_pr = 1;
    else if (digitalRead(BUTTON_C) == 0) btn_pr = 2;
    if(btn_pr != last_btn) {               /* Is there a button press? (E.g. -1 indicated no button was pressed) */
       last_btn = btn_pr;           /* Save the ID of the pressed button */
       if (btn_pr >= 0) {
          data->state = LV_INDEV_STATE_PRESSED;  /* Set the pressed state */
        } else {
          data->state = LV_INDEV_STATE_RELEASED; /* Set the released state */
        }
    }
    data->btn_id = last_btn;         /* Save the last button */
} /* my_button_read() */

static void event_handler_a(lv_event_t * e)
{
  lv_label_set_text(msg_label, "You pressed A");
}

static void event_handler_b(lv_event_t * e)
{
  lv_label_set_text(msg_label, "You pressed B");
}

static void event_handler_c(lv_event_t * e)
{
  lv_label_set_text(msg_label, "You pressed C");
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
    return millis();
}

void setup()
{
  int rc;

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.begin( 115200 );
    delay(3000);
    Serial.println( LVGL_Arduino );
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(my_tick);

    /* register print function for debugging */
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print );
#endif

    lv_display_t * disp;
    int w, h, iSize;
//    epaper.initPanel(BB_PANEL_M5PAPERS3);
   epaper.initPanel(BB_PANEL_V7_RAW);
   epaper.setPanelSize(1280, 720, BB_PANEL_FLAG_MIRROR_Y);
    // If grayscale or color mode, set FastEPD to 16-gray mode
  //#if LV_COLOR_DEPTH > 1
  //  epaper.setMode(BB_MODE_4BPP);
  //#endif
    epaper.clearWhite();
    w = epaper.width();
    h = epaper.height();
    iSize = DRAW_BUF_SIZE(w, h);
    draw_buf = (uint16_t *)malloc(iSize);
    disp = lv_display_create(w, h);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, iSize, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
    /* Create a simple label
     * ---------------------
     lv_obj_t *label = lv_label_create( lv_screen_active() );
     lv_label_set_text( label, "Hello Arduino, I'm LVGL!" );
     lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

     * Try an example. See all the examples
     *  - Online: https://docs.lvgl.io/master/examples.html
     *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
     * ----------------------------------------------------------------

     lv_example_btn_1();

     * Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMO_WIDGETS
     * -------------------------------------------------------------------------------------------

     lv_demo_widgets();
     */

    label = lv_label_create( lv_screen_active() );
    lv_label_set_text( label, "LVGL Physical Button Demo" );
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

    msg_label = lv_label_create( lv_screen_active() );
    lv_label_set_text( msg_label, "Press a button" );
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align( msg_label, LV_ALIGN_CENTER, 0, +60 );

    btn1 = lv_button_create(lv_screen_active());
//    lv_obj_add_flag(btn1, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn1, 300, 200);
    lv_obj_set_pos(btn1, 10, 10);
    lv_obj_add_event_cb(btn1, event_handler_a, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_PRESSED);
    label_a = lv_label_create(btn1);
    lv_obj_set_style_text_font(label_a, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_label_set_text(label_a, "A");
    lv_obj_center(label_a);

    btn2 = lv_button_create(lv_screen_active());
//    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn2, 300, 200);
    lv_obj_set_pos(btn2, 300+10+180, 10);
    lv_obj_add_event_cb(btn2, event_handler_b, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn2, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_PRESSED);
    label_b = lv_label_create(btn2);
    lv_obj_set_style_text_font(label_b, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_label_set_text(label_b, "B");
    lv_obj_center(label_b);
//    lv_obj_set_style_text_color(label_b, lv_color_make(0,0,0), 0);

    btn3 = lv_button_create(lv_screen_active());
//    lv_obj_add_flag(btn3, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn3, 300, 200);
    lv_obj_set_pos(btn3, 600+10+180*2, 10);
    lv_obj_add_event_cb(btn3, event_handler_c, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn3, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn3, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_PRESSED);
    label_c = lv_label_create(btn3);
    lv_obj_set_style_text_font(label_c, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_label_set_text(label_c, "C");
    lv_obj_center(label_c);
//    lv_obj_set_style_text_color(label_c, lv_color_make(0,0,0), 0);

    /* Initialize the input device */
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_BUTTON); // use the physical buttons of the Tufty
    lv_indev_set_read_cb(indev, my_button_read);
    lv_indev_set_button_points(indev, points_array);

    Serial.println( "Setup done" );
} /* setup() */

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}
