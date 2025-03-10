/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */
#include <lvgl.h>
#include <FastEPD.h>
#include <bb_captouch.h>
FASTEPD epaper;
BBCapTouch bbct;
lv_obj_t *label1, *slider1;

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.
 *Note that the `lv_examples` library is for LVGL v7 and you shouldn't install it for this version (since LVGL v8)
 *as the examples and demos are now part of the main LVGL library. */

//#include <examples/lv_examples.h>
//#include <demos/lv_demos.h>

#define DRAW_BUF_SIZE(w, h) ((w * h) / 10 * sizeof(uint16_t))
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
            epaper.partialUpdate(true, iTop, iBottom);
            iTop = 2000; iBottom = 0; // reset top/bottom row checks
        } else { // 16-gray mode only supports full updates, or after 50 partial updates
            epaper.fullUpdate(false, false);
            iUpdates = 0;
        }
        iUpdates++;
    }
}

/*Read the touchpad*/
void my_captouch_read( lv_indev_t * indev, lv_indev_data_t * data )
{
  TOUCHINFO ti;

  if (bbct.getSamples(&ti) && ti.count > 0) {
       data->state = LV_INDEV_STATE_PRESSED;
       data->point.x = ti.y[0];
       data->point.y = epaper.height() - 1 - ti.x[0];
  } else {
      data->state = LV_INDEV_STATE_RELEASED;
  }
} /* my_captouch_read() */

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target_obj(e);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d C", (int)lv_slider_get_value(slider));
    lv_label_set_text(label1, buf);
    Serial.println(buf);
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

    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(my_tick);

    /* register print function for debugging */
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print );
#endif

    lv_display_t * disp;
    int w, h, iSize;
    epaper.initPanel(BB_PANEL_M5PAPERS3);
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

    label1 = lv_label_create( lv_screen_active() );
    lv_label_set_text( label1, "20 C" );
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_align( label1, LV_ALIGN_CENTER, 0, -120 );

    slider1 = lv_slider_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(slider1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_height(slider1, 60);
    lv_slider_set_mode(slider1, LV_SLIDER_MODE_RANGE);
    lv_slider_set_range(slider1, 10, 35); // min/max value
    lv_slider_set_value(slider1, 20, LV_ANIM_OFF);
//    lv_slider_set_start_value(slider1, 20, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider1, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_refresh_ext_draw_size(slider1);

    Serial.println( "Setup done" );
// Initialize the touch library
    rc = bbct.init(CONFIG_M5_PAPERS3); // GPIO numbers are already in the library for this device
    if (rc == CT_SUCCESS) {
      /*Initialize the input device driver*/
        lv_indev_t * indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
        lv_indev_set_read_cb(indev, my_captouch_read);
        Serial.println("bb_captouch started successfully");
    } else {
        Serial.println("Touch init failed");
    }

    Serial.println( "Setup done" );
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}
