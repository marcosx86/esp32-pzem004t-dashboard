#include <Arduino.h>
#include <lvgl.h>
#include "Display_LGFX.h"
#include "CST820.h"
#include "Touch_Calibrator.h"
#include "Network_Manager.h"
#include "Dashboard_UI.h"

#define GFX
#define TOUCH 
// Define TOUCH_CALIBRATION to enable manual touchscreen calibration if NVS values are missing on boot
#define TOUCH_CALIBRATION

// Configure these for your network and target metrics endpoint
#define WIFI_SSID "hAP AC Lite 2G"
#define WIFI_PASS "123mudar"
#define METRICS_URL "http://192.168.81.16/metrics"

/* Change screen resolution (landscape) */
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

/* Define touchscreen pins */
#define I2C_SDA 21
#define I2C_SCL 22

#ifdef TOUCH
CST820 touch(I2C_SDA, I2C_SCL, -1, -1); /* Touch Instance */

// Global calibration coefficients
static double cal_A = 0.0;
static double cal_B = 1.0;
static double cal_C = 0.0;
static double cal_D = -1.0;
static double cal_E = 0.0;
static double cal_F = 240.0;
static bool touch_calibrated = false;
#endif

static lv_disp_draw_buf_t draw_buf;
lv_indev_t *myInputDevice;

#ifdef GFX
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
    tft.writePixels(&color_p->full, w * h, false);

    tft.endWrite();            /* terminate TFT transaction */
    lv_disp_flush_ready(disp); /* tell lvgl that flushing is done */
}

#ifdef TOUCH
/* Read touchpad callback */
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    bool touched;
    uint8_t gesture;
    uint16_t touchX, touchY;

    touched = touch.getTouch(&touchX, &touchY, &gesture);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
        if (DashboardUI::touch_cursor) {
            lv_obj_add_flag(DashboardUI::touch_cursor, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        // Apply affine transformation calibration to raw touch coordinates
        int16_t calibrated_x, calibrated_y;
        if (touch_calibrated) {
            calibrated_x = (int16_t)(cal_A * touchX + cal_B * touchY + cal_C);
            calibrated_y = (int16_t)(cal_D * touchX + cal_E * touchY + cal_F);
        } else {
            // Default mapping assuming 90-degree clockwise landscape rotation
            calibrated_x = (int16_t)touchY;
            calibrated_y = (int16_t)(240 - touchX);
        }

        // Constrain coordinates to screen boundaries
        if (calibrated_x < 0) calibrated_x = 0;
        if (calibrated_x >= TFT_WIDTH) calibrated_x = TFT_WIDTH - 1;
        if (calibrated_y < 0) calibrated_y = 0;
        if (calibrated_y >= TFT_HEIGHT) calibrated_y = TFT_HEIGHT - 1;
        
        data->point.x = calibrated_x;
        data->point.y = calibrated_y;

        // Print touch coordinates for diagnostic purposes (rate-limited to 100ms)
        static uint32_t last_print = 0;
        if (millis() - last_print > 100) {
            Serial.printf("[Touch] Raw: (%d, %d) -> Calibrated: (%d, %d)\n", touchX, touchY, calibrated_x, calibrated_y);
            last_print = millis();
        }
        
        // Update and show touch cursor
        if (DashboardUI::touch_cursor) {
            lv_obj_set_pos(DashboardUI::touch_cursor, calibrated_x - 10, calibrated_y - 10);  // Center cursor on touch point
            lv_obj_clear_flag(DashboardUI::touch_cursor, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
#endif
#endif

#if LV_USE_LOG != 0
void my_print(const char * buf) {
    Serial.print(buf);
}
#endif

void setup()
{
    Serial.begin(115200);
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH);

#ifdef TOUCH
    touch.begin(); /* Initialize touchpad */
#endif

#if defined(GFX)
    lv_init();
#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); // register print function for debugging
#endif

    tft.init();
    tft.fillScreen(TFT_BLACK);

    // Reduced buffer size to save internal DMA memory and prevent allocation failures
    size_t buf_size = TFT_WIDTH * 40;
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (buf1 == NULL || buf2 == NULL) {
        Serial.println("Error: Failed to allocate DMA rendering buffers! Trying standard heap...");
        if (buf1 == NULL) buf1 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
        if (buf2 == NULL) buf2 = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
    }

    if (buf1 == NULL || buf2 == NULL) {
        Serial.println("Fatal Error: Could not allocate any display buffers!");
        while(1) { delay(1000); }
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    // Initialize display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
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

    // Load calibration from NVS, or run calibration if not found
    bool has_cal = loadTouchCalibration(cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, touch_calibrated);
    if (!has_cal) {
#ifdef TOUCH_CALIBRATION
        runTouchCalibration(tft, touch, cal_A, cal_B, cal_C, cal_D, cal_E, cal_F, touch_calibrated);
#else
        Serial.println("No calibration in NVS and manual calibration compile flag is disabled. Using default rotation.");
#endif
    }
#endif

    tft.fillScreen(TFT_BLACK);
    delay(100);
    tft.fillScreen(TFT_RED);
    delay(100);
    tft.fillScreen(TFT_GREEN);
    delay(100);
    tft.fillScreen(TFT_BLUE);
    delay(100);
    tft.fillScreen(TFT_BLACK);

    // Start background network fetch and initialize the UI dashboard layout
    NetworkManager::begin(WIFI_SSID, WIFI_PASS, METRICS_URL);
    DashboardUI::init(TFT_WIDTH, TFT_HEIGHT);

    // Setup update timer
    auto update_timer_cb = [](lv_timer_t *t){
        MetricsData m;
        if (NetworkManager::getLatestMetrics(m)) {
            DashboardUI::update(m);
        }
    };
    lv_timer_create(update_timer_cb, 1000, NULL);
#endif
}

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5);
}

void app_main(void) {
    setup();

    while (1) {
        loop();
    }
}