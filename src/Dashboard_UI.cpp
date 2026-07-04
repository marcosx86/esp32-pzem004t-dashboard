#include "Dashboard_UI.h"

namespace DashboardUI {
    // Global touch cursor pointer
    lv_obj_t *touch_cursor = NULL;

    // Static variables for UI tracking
    static lv_obj_t *card_value[4];
    static lv_obj_t *card_obj[4];
    static int zoom_card_index = -1;
    static lv_obj_t *zoom_overlay = NULL;
    static lv_obj_t *zoom_detail = NULL;
    
    static uint16_t screen_width = 320;
    static uint16_t screen_height = 240;

    // Forward declarations of internal helpers
    static void zoom_in(int card_idx);
    static void zoom_out();
    static void card_event_cb(lv_event_t * e);

    static void card_event_cb(lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            lv_obj_t *obj = lv_event_get_target(e);
            for (int i = 0; i < 4; i++) {
                if (card_obj[i] == obj) {
                    if (zoom_card_index == i) {
                        zoom_out();
                    } else {
                        zoom_in(i);
                    }
                    break;
                }
            }
        }
    }

    static void zoom_in(int card_idx) {
        if (zoom_card_index != -1) zoom_out();
        
        zoom_card_index = card_idx;
        
        // Dimmed overlay
        zoom_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(zoom_overlay, screen_width, screen_height);
        lv_obj_set_align(zoom_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(zoom_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(zoom_overlay, LV_OPA_50, 0);
        lv_obj_clear_flag(zoom_overlay, LV_OBJ_FLAG_CLICKABLE);
        
        // Fullscreen details card
        zoom_detail = lv_obj_create(lv_scr_act());
        lv_obj_set_size(zoom_detail, screen_width - 20, screen_height - 20);
        lv_obj_set_align(zoom_detail, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(zoom_detail, 12, 0);
        lv_obj_set_style_border_width(zoom_detail, 3, 0);
        lv_obj_set_style_border_color(zoom_detail, lv_color_hex(0x333333), 0);
        lv_obj_set_style_pad_all(zoom_detail, 20, 0);
        
        static uint32_t colors[] = {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12};
        lv_obj_set_style_bg_color(zoom_detail, lv_color_hex(colors[card_idx]), 0);
        lv_obj_set_style_bg_opa(zoom_detail, LV_OPA_COVER, 0);
        
        // Title
        lv_obj_t *title = lv_label_create(zoom_detail);
        static const char* titles[] = {"Voltage (V)", "Current (A)", "Energy (kWh)", "Power (W)"};
        lv_label_set_text(title, titles[card_idx]);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
        
        // Value
        lv_obj_t *detail_value = lv_label_create(zoom_detail);
        const char *current_val = lv_label_get_text(card_value[card_idx]);
        lv_label_set_text(detail_value, current_val);
        lv_obj_set_style_text_font(detail_value, &lv_font_montserrat_48, 0);
        lv_obj_align(detail_value, LV_ALIGN_CENTER, 0, 0);
        
        lv_obj_set_user_data(zoom_detail, detail_value);
        
        lv_obj_add_flag(zoom_detail, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(zoom_detail, [](lv_event_t * e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                zoom_out();
            }
        }, LV_EVENT_CLICKED, NULL);
    }

    static void zoom_out() {
        if (zoom_overlay) {
            lv_obj_del(zoom_overlay);
            zoom_overlay = NULL;
        }
        if (zoom_detail) {
            lv_obj_del(zoom_detail);
            zoom_detail = NULL;
        }
        zoom_card_index = -1;
    }

    void init(uint16_t tft_width, uint16_t tft_height) {
        screen_width = tft_width;
        screen_height = tft_height;

        // Create visual touch pointer (small circle that follows finger)
        touch_cursor = lv_obj_create(lv_scr_act());
        lv_obj_set_size(touch_cursor, 20, 20);
        lv_obj_set_style_radius(touch_cursor, 10, 0);  // Make it circular
        lv_obj_set_style_bg_color(touch_cursor, lv_color_hex(0xFF0000), 0);  // Red circle
        lv_obj_set_style_bg_opa(touch_cursor, LV_OPA_80, 0);  // Semi-transparent
        lv_obj_add_flag(touch_cursor, LV_OBJ_FLAG_HIDDEN);  // Hidden by default (until touch)

        // Helper lambda to create simple card
        auto create_card = [](const char *title, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, uint32_t bg_color, lv_obj_t **out_value){
            const int M = 8; // outer margin and spacing
            int card_w = (screen_width - 3 * M) / 2;
            int card_h = (screen_height - 3 * M) / 2;

            lv_obj_t *c = lv_obj_create(lv_scr_act());
            lv_obj_set_size(c, card_w, card_h);
            lv_obj_set_align(c, align);

            // styling
            lv_obj_set_style_pad_all(c, 10, 0);
            lv_obj_set_style_radius(c, 8, 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(0x444444), 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(bg_color), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);

            lv_obj_t *t = lv_label_create(c);
            lv_label_set_text(t, title);
            lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
            lv_obj_align(t, LV_ALIGN_TOP_LEFT, 10, 6);

            lv_obj_t *v = lv_label_create(c);
            lv_label_set_text(v, "--");
            lv_obj_set_style_text_font(v, &lv_font_montserrat_22, 0);
            lv_obj_align(v, LV_ALIGN_CENTER, 0, 10);

            // Make card clickable
            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, card_event_cb, LV_EVENT_CLICKED, NULL);

            // Apply positions for margins
            lv_obj_set_pos(c, x_ofs, y_ofs);

            if (out_value) *out_value = v;
            return c;
        };

        // Create 2x2 cards layout
        card_obj[0] = create_card("Voltage (V)", LV_ALIGN_TOP_LEFT, 5, 5, 0x4A90E2, &card_value[0]);      // Blue
        card_obj[1] = create_card("Current (A)", LV_ALIGN_TOP_RIGHT, -5, 5, 0xE74C3C, &card_value[1]);    // Red
        card_obj[2] = create_card("Energy (kWh)", LV_ALIGN_BOTTOM_LEFT, 5, -5, 0x2ECC71, &card_value[2]); // Green
        card_obj[3] = create_card("Power (W)", LV_ALIGN_BOTTOM_RIGHT, -5, -5, 0xF39C12, &card_value[3]);  // Orange
    }

    void update(const MetricsData &data) {
        if (!isnan(data.voltage)) {
            String s = String(data.voltage, 2) + " V";
            lv_label_set_text(card_value[0], s.c_str());
        } else lv_label_set_text(card_value[0], "--");

        if (!isnan(data.current)) {
            String s = String(data.current, 2) + " A";
            lv_label_set_text(card_value[1], s.c_str());
        } else lv_label_set_text(card_value[1], "--");

        if (!isnan(data.energy)) {
            String s = String(data.energy, 2) + " kWh";
            lv_label_set_text(card_value[2], s.c_str());
        } else lv_label_set_text(card_value[2], "--");

        if (!isnan(data.wattage)) {
            String s = String(data.wattage, 2) + " W";
            lv_label_set_text(card_value[3], s.c_str());
        } else lv_label_set_text(card_value[3], "--");

        // Update zoomed detail view if active
        if (zoom_detail && zoom_card_index >= 0) {
            lv_obj_t *detail_val = (lv_obj_t*)lv_obj_get_user_data(zoom_detail);
            if (detail_val) {
                lv_label_set_text(detail_val, lv_label_get_text(card_value[zoom_card_index]));
            }
        }
    }
}
