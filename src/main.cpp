#include <Arduino.h>
#include <lvgl.h>
#include <demos/lv_demos.h>
#include "CST820.h"
#include <WiFi.h>
#include "CST820_metrics.h"
#include <cmath>
#define GFX
// Uncomment the line below if touch-to-unlock is included.
// #define TOUCH 

// Configure these for your network and target metrics endpoint
#define WIFI_SSID "hAP AC Lite 2G"
#define WIFI_PASS "123mudar"
// Full URL including http:// and /metrics path
#define METRICS_URL "http://192.168.81.16/metrics"

/* Change screen resolution (landscape) */
// Swap width/height for landscape orientation
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

/* Define touchscreen pins */
#define I2C_SDA 21
#define I2C_SCL 22

#ifdef TOUCH
CST820 touch(I2C_SDA, I2C_SCL, -1, -1); /* Touch Examples */
#endif

static lv_disp_draw_buf_t draw_buf;
// static lv_color_t buf[TFT_WIDTH * 100];
lv_indev_t *myInputDevice;
static lv_obj_t *card_value[4];

#if defined(GFX)
#define LGFX_USE_V1      // set to use new version of library
#include <LovyanGFX.hpp> // main library

class LGFX : public lgfx::LGFX_Device
{

    lgfx::Panel_ST7789 _panel_instance; // ST7789UI
    lgfx::Bus_Parallel8 _bus_instance;  // MCU8080 8B

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 25000000;
            cfg.pin_wr = 4;
            cfg.pin_rd = 2;
            cfg.pin_rs = 16;

            cfg.pin_d0 = 15;
            cfg.pin_d1 = 13;
            cfg.pin_d2 = 12;
            cfg.pin_d3 = 14;
            cfg.pin_d4 = 27;
            cfg.pin_d5 = 25;
            cfg.pin_d6 = 33;
            cfg.pin_d7 = 32;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();

            cfg.pin_cs = 17;
            cfg.pin_rst = -1;
            cfg.pin_busy = -1;

            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            /* Rotate display so logical coordinates match landscape */
            cfg.offset_rotation = 1;
            // cfg.dummy_read_pixel = 8;
            // cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};

static LGFX tft; // declare display variable

#endif

#if defined(GFX)
/*
    lcd interface
    transfer pixel data range to lcd
*/
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);

    tft.startWrite();                            /* Start new TFT transaction */
    tft.setAddrWindow(area->x1, area->y1, w, h); /* set the working window */
    tft.writePixels(&color_p->full, w * h,false);//true

    tft.endWrite();            /* terminate TFT transaction */
    lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
}

#ifdef TOUCH
/*读取触摸板*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{

    bool touched;
    uint8_t gesture;
    uint16_t touchX, touchY;

    touched = touch.getTouch(&touchX, &touchY, &gesture);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;
    }
}
#endif
#endif


void setup()
{
    Serial.begin(115200);
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH);

#ifdef TOUCH
    touch.begin(); /*初始化触摸板*/
#endif

#if defined(GFX)
    lv_init();
    // lv_img_cache_set_size(10);
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); // register print function for debugging
#endif

    tft.init();
    tft.fillScreen(TFT_BLACK);

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(TFT_HEIGHT * 150 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(TFT_HEIGHT * 150 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, TFT_HEIGHT * 150);

    // initialise the display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    // settings for display driver
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

#ifdef TOUCH
    /* Initialize (virtual) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
#endif

    tft.fillScreen(TFT_BLACK);
    delay(50);
    tft.fillScreen(TFT_RED);
    delay(50);
    tft.fillScreen(TFT_GREEN);
    delay(50);
    tft.fillScreen(TFT_BLUE);
    delay(50);

    //lv_demo_widgets();
    //lv_demo_benchmark();
    
    // --- Metrics Dashboard Start ---
    // Connect to WiFi
    Serial.printf("Connecting to WiFi '%s'", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
        delay(200);
        Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" connected");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(" failed to connect");
    }

    // helper to create simple card with title and value label
    auto create_card = [](const char *title, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, uint32_t bg_color, lv_obj_t **out_value){
        const int M = 8; // outer margin and spacing
        int card_w = (TFT_WIDTH - 3 * M) / 2;
        int card_h = (TFT_HEIGHT - 3 * M) / 2;

        lv_obj_t *c = lv_obj_create(lv_scr_act());
        lv_obj_set_size(c, card_w, card_h);
        lv_obj_set_align(c, align);

        // card styling
        lv_obj_set_style_pad_all(c, 10, 0);
        lv_obj_set_style_radius(c, 8, 0);
        lv_obj_set_style_border_width(c, 2, 0);
        lv_obj_set_style_border_color(c, lv_color_hex(0x444444), 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(bg_color), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);

        lv_obj_t *t = lv_label_create(c);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);  // Title font size 16
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 10, 6);

        lv_obj_t *v = lv_label_create(c);
        lv_label_set_text(v, "--");
        lv_obj_set_style_text_font(v, &lv_font_montserrat_24, 0);  // Value font size 28
        lv_obj_align(v, LV_ALIGN_CENTER, 0, 10);

        if (out_value) *out_value = v;
        return c;
    };

    // create 4 cards in a 2x2 layout with distinct colors
    create_card("Voltage (V)", LV_ALIGN_TOP_LEFT, 15, 15, 0x4A90E2, &card_value[0]);      // Blue
    create_card("Current (A)", LV_ALIGN_TOP_RIGHT, -15, 15, 0xE74C3C, &card_value[1]);    // Red
    create_card("Energy (kWh)", LV_ALIGN_BOTTOM_LEFT, 15, -15, 0x2ECC71, &card_value[2]); // Green
    create_card("Power (W)", LV_ALIGN_BOTTOM_RIGHT, -15, -15, 0xF39C12, &card_value[3]);  // Orange

    // timer callback to fetch metrics and update labels
    auto update_cb = [](lv_timer_t *t){
        MetricsData m;
        if (fetch_metrics(METRICS_URL, m)) {
            if (!isnan(m.voltage)) {
                String s = String(m.voltage, 2) + " V";
                lv_label_set_text(card_value[0], s.c_str());
            } else lv_label_set_text(card_value[0], "--");

            if (!isnan(m.current)) {
                String s = String(m.current, 2) + " A";
                lv_label_set_text(card_value[1], s.c_str());
            } else lv_label_set_text(card_value[1], "--");

            if (!isnan(m.energy)) {
                String s = String(m.energy, 2) + " kWh";
                lv_label_set_text(card_value[2], s.c_str());
            } else lv_label_set_text(card_value[2], "--");

            if (!isnan(m.wattage)) {
                String s = String(m.wattage, 2) + " W";
                lv_label_set_text(card_value[3], s.c_str());
            } else lv_label_set_text(card_value[3], "--");
        } else {
            // network or parse failure: mark values stale
            for (int i = 0; i < 4; i++) lv_label_set_text(card_value[i], "--");
        }
    };

    // create timer every 1000ms
    lv_timer_create(update_cb, 1000, NULL);

    // First update will run on the first timer tick (after ~1s)
    // --- Metrics Dashboard End ---
#endif
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(333);
}