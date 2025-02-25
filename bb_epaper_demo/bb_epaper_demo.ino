/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */
#include <lvgl.h>
#include <bb_epaper.h>
BBEPAPER bbep(EP81_SPECTRA_1024x576);
#define DC_PIN 14
#define BUSY_PIN 13
#define RESET_PIN 9
#define CS_PIN 10
#define CS_PIN2 8
#define SCK_PIN 12
#define MOSI_PIN 11

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

#ifdef __BB_EPAPER__
// it's fastest to use a small lookup table for the RGB565-> Spectra6 colors
// RGB565->RGB111
// We have to translate some the BBEP colors because we're writing straight into the framebuffer
const uint8_t u8Colors[8] = {BBEP_BLACK, 0x5, 0x6, 0x6, BBEP_RED, 0x5, BBEP_YELLOW, BBEP_WHITE};
/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Copy `px map` to the `area`*/
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    uint16_t u16, *s = (uint16_t *)px_map;
    uint8_t ucL, ucR, *d; // epaper buffer is 4-bits per pixel

    for (int y=0; y<h; y++) {
        d = (uint8_t *)bbep.getBuffer();
        d += (bbep.width()/2) * (y + area->y1);
        d += (area->x1 / 2);
        for (int x=0; x<w; x+=2) {
          u16 = s[x]; // get the RGB565 pixel
          ucL = (u16 >> 4) & 1; // blue
          ucL |= ((u16 >> 9) & 2); // green
          ucL |= ((u16 >> 13) & 4); // red
          ucL = u8Colors[ucL]; // convert to Spectra 6 color
          u16 = s[x+1]; // get the RGB565 pixel
          ucR = (u16 >> 4) & 1; // blue
          ucR |= ((u16 >> 9) & 2); // green
          ucR |= ((u16 >> 13) & 4); // red
          ucR = u8Colors[ucR]; // convert to Spectra 6 color
          *d++ = (ucL << 4) | ucR; // store a pair of pixels
        } // for x
        s += w;
    } // for y

    /*tell LVGL you are ready*/
    lv_display_flush_ready(disp);
    if (lv_disp_flush_is_last(disp)) { // time to update the eink
        bbep.writePlane();
        bbep.refresh(REFRESH_FULL);
    }
}
#endif // bb_epaper

/* Read the touch sensor */
void my_touch_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    /*For example  ("my_..." functions needs to be implemented by you)
    int32_t x, y;
    bool touched = false; //my_get_touch( &x, &y );

    if(!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;

        data->point.x = x;
        data->point.y = y;
    }
     */
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
    return millis();
}

void setup()
{
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
    bbep.initIO(DC_PIN, RESET_PIN, BUSY_PIN, CS_PIN);
    bbep.setCS2(CS_PIN2);
    bbep.setRotation(0);
    bbep.allocBuffer();
    w = bbep.width();
    h = bbep.height();
    iSize = DRAW_BUF_SIZE(w, h);
    draw_buf = (uint16_t *)malloc(iSize);
    disp = lv_display_create(w, h);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, iSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /*Initialize the (dummy) input device driver*/
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touch_read);

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
//    lv_obj_t *scr = lv_scr_act();
//    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // set background color to black
//    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *label1, *label2, *label3;
    label1 = lv_label_create( lv_screen_active() );
    lv_label_set_text( label1, "Hello Arduino, I'm LVGL!" );
    lv_obj_align( label1, LV_ALIGN_CENTER, 0, 0 );

    lv_obj_t * btn1 = lv_button_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
    lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(btn1, 40);
    lv_obj_set_style_bg_color(btn1, lv_color_make(0,0xff,0), LV_PART_MAIN); // green
    Serial.println( "Setup done" );

    lv_obj_t * btn2 = lv_button_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, -40, -40);
    lv_obj_remove_flag(btn2, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(btn2, 40);
    lv_obj_set_style_bg_color(btn2, lv_color_make(0xff,0xff,0), LV_PART_MAIN); // yellow

    lv_obj_t * btn3 = lv_button_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn3, LV_ALIGN_CENTER, +40, -40);
    lv_obj_remove_flag(btn3, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(btn3, 40);
    lv_obj_set_style_bg_color(btn3, lv_color_make(0,0,0xff), LV_PART_MAIN); // blue

    lv_obj_t * btn4 = lv_button_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn4, LV_ALIGN_CENTER, -80, -40);
    lv_obj_remove_flag(btn4, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(btn4, 40);
    lv_obj_set_style_bg_color(btn4, lv_color_make(0xff,0,0), LV_PART_MAIN); // red

    lv_obj_t * btn5 = lv_button_create(lv_screen_active());
    //lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn5, LV_ALIGN_CENTER, +80, -40);
    lv_obj_remove_flag(btn5, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_height(btn5, 40);
    lv_obj_set_style_bg_color(btn5, lv_color_make(0,0,0), LV_PART_MAIN); // black

    Serial.println( "Setup done" );
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}
