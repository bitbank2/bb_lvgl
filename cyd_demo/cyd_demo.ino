//
// LVGL demo for CYDs (Cheap Yellow Displays)
// written by Larry Bank (bitbank@pobox.com)
// Feb 25, 2025
// This demo makes use of my display and touch libraries:
// https://github.com/bitbank2/bb_spi_lcd
// https://github.com/bitbank2/bb_captouch
//
#include <lvgl.h>
#include <bb_spi_lcd.h>
#include <bb_captouch.h>
BBCapTouch bbct;
BB_SPI_LCD lcd;
uint16_t dma_buf[512];
lv_obj_t *label, *msg_label, *label_a, *label_b, *label_c;
lv_obj_t *btn1, *btn2, *btn3;
lv_obj_t *scr;
// GPIO assignments for the capactive touch sensor of the JC4827W543
#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_RST 38
#define TOUCH_INT 3

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
/* Arduino devices are almost exclusively little-endian machines, but SPI */
/* LCDs are big endian, so we need to swap the byte order */
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Copy `px map` to the `area`*/
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    uint16_t *s = (uint16_t *)px_map;
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
          dma_buf[x] = __builtin_bswap16(s[x]); // convert to big-endian
        }
        lcd.pushPixels(dma_buf, w, DRAW_TO_LCD | DRAW_WITH_DMA);
        s += w;
    }

    /*tell LVGL you are ready*/
    lv_display_flush_ready(disp);
} /* my_disp_flush */

/* Read the touch sensor */
void my_touch_read( lv_indev_t * indev, lv_indev_data_t * data )
{
TOUCHINFO ti;
    if (bbct.getSamples(&ti) && ti.count >= 1) { // a touch event
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = ti.x[0];
        data->point.y = ti.y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
} /* my_touch_read() */

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
    lcd.begin(DISPLAY_CYD_543); // the JC4827W543 (with optional capactive or resistive touch sensor)
    w = lcd.width();
    h = lcd.height();
    // Initialize the capacitive touch sensor library
    bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
    iSize = DRAW_BUF_SIZE(w, h);
    draw_buf = (uint16_t *)malloc(iSize);
    disp = lv_display_create(w, h);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, iSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Initialize the input device */
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
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // set background color to black
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    label = lv_label_create( lv_screen_active() );
    lv_label_set_text( label, "LVGL Touch Button Demo" );
    lv_obj_set_style_text_color(label, lv_color_make(0,0xff,0), 0);
    lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

    msg_label = lv_label_create( lv_screen_active() );
    lv_label_set_text( msg_label, "Press a button" );
    lv_obj_set_style_text_color(msg_label, lv_color_make(0xff,0xff,0xff), 0);
    lv_obj_align( msg_label, LV_ALIGN_CENTER, 0, +60 );

    btn1 = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn1, 80, 60);
    lv_obj_align( btn1, LV_ALIGN_CENTER, -150, -100);
    lv_obj_add_event_cb(btn1, event_handler_a, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn1, lv_color_make(0,0xff,0), LV_PART_MAIN); // green
    label_a = lv_label_create(btn1);
    lv_label_set_text(label_a, "A");
    lv_obj_center(label_a);
    lv_obj_set_style_text_color(label_a, lv_color_make(0,0,0), 0);

    btn2 = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn2, 80, 60);
    lv_obj_align(btn2 , LV_ALIGN_CENTER, 0, -100 );
    lv_obj_add_event_cb(btn2, event_handler_b, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn2, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn2, lv_color_make(0xff,0xff,0), LV_PART_MAIN); // yellow
    label_b = lv_label_create(btn2);
    lv_label_set_text(label_b, "B");
    lv_obj_center(label_b);
    lv_obj_set_style_text_color(label_b, lv_color_make(0,0,0), 0);

    btn3 = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn3, 80, 60);
    lv_obj_align(btn3, LV_ALIGN_CENTER, 150, -100 );
    lv_obj_add_event_cb(btn3, event_handler_c, LV_EVENT_PRESSED, NULL);
    lv_obj_remove_flag(btn3, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_style_bg_color(btn3, lv_color_make(0,0,0xff), LV_PART_MAIN); // blue
    label_c = lv_label_create(btn3);
    lv_label_set_text(label_c, "C");
    lv_obj_center(label_c);
    lv_obj_set_style_text_color(label_c, lv_color_make(0,0,0), 0);

    Serial.println( "Setup done" );
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}

