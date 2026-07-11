#include "Dashboard_UI.h"
#include "Network_Manager.h"
#include "Touch_Calibrator.h"
#include "Display_LGFX.h"
#include <Preferences.h>

namespace DashboardUI {
    // Global touch cursor pointer
    lv_obj_t *touch_cursor = NULL;

    // Static variables for UI tracking
    static lv_obj_t *card_value[4];
    static lv_obj_t *card_obj[4];
    static int zoom_card_index = -1;
    static lv_obj_t *zoom_overlay = NULL;
    static lv_obj_t *zoom_detail = NULL;
    // static lv_obj_t *settings_overlay = NULL;
    static lv_obj_t *settings_content = NULL;
    static lv_obj_t *settings_list = NULL;
    
    // Editor UI
    static lv_obj_t *editor_panel = NULL;
    static lv_obj_t *wifi_edit_overlay = NULL;
    static lv_obj_t *ssid_editor = NULL;
    static lv_obj_t *password_editor = NULL;
    static lv_obj_t *keyboard = NULL;
    static int editing_index = -1;

    // Metrics UI
    static lv_obj_t *metrics_ip_ta = NULL;
    static lv_obj_t *metrics_path_ta = NULL;

    // Error UI
    static lv_obj_t *fetch_error_icon = NULL;

    // Brightness UI
    static lv_obj_t *brightness_overlay = NULL;
    static int initial_brightness_val = 128;

    static uint16_t screen_width = 320;
    static uint16_t screen_height = 240;

    // Forward declarations of internal helpers
    static void zoom_in(int card_idx);
    static void zoom_out();
    static void card_event_cb(lv_event_t * e);
    static void refresh_settings_list();
    static void hide_network_editor();
    static void show_network_editor(int index);
    static void settings_close_event_cb(lv_event_t * e);
    static void settings_add_event_cb(lv_event_t * e);
    static void network_row_event_cb(lv_event_t * e);
    static void network_delete_event_cb(lv_event_t * e);
    static void network_confirm_event_cb(lv_event_t * e);
    static void network_cancel_event_cb(lv_event_t * e);
    static void network_connect_event_cb(lv_event_t * e);
    static void textarea_event_cb(lv_event_t * e);
    static void show_brightness_slider();

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

        zoom_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(zoom_overlay, screen_width, screen_height);
        lv_obj_set_align(zoom_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(zoom_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(zoom_overlay, LV_OPA_50, 0);
        lv_obj_set_style_border_width(zoom_overlay, 0, 0);
        lv_obj_set_style_radius(zoom_overlay, 0, 0);
        lv_obj_clear_flag(zoom_overlay, LV_OBJ_FLAG_CLICKABLE);

        zoom_detail = lv_obj_create(zoom_overlay);
        lv_obj_set_size(zoom_detail, screen_width - 40, screen_height - 120);
        lv_obj_set_align(zoom_detail, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(zoom_detail, 12, 0);
        lv_obj_set_style_border_width(zoom_detail, 3, 0);
        lv_obj_set_style_border_color(zoom_detail, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_all(zoom_detail, 10, 0);
        lv_obj_set_scrollbar_mode(zoom_detail, LV_SCROLLBAR_MODE_OFF);

        static uint32_t colors[] = {0x4A90E2, 0xE74C3C, 0x2ECC71, 0xF39C12};
        lv_obj_set_style_bg_color(zoom_detail, lv_color_hex(colors[card_idx]), 0);
        lv_obj_set_style_bg_opa(zoom_detail, LV_OPA_COVER, 0);

        lv_obj_t *title = lv_label_create(zoom_detail);
        static const char* titles[] = {"Voltage (V)", "Current (A)", "Energy (kWh)", "Power (W)"};
        lv_label_set_text(title, titles[card_idx]);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

        lv_obj_t *detail_value = lv_label_create(zoom_detail);
        const char *current_val = lv_label_get_text(card_value[card_idx]);
        lv_label_set_text(detail_value, current_val);
        lv_obj_set_style_text_font(detail_value, &lv_font_montserrat_44, 0);
        lv_obj_align(detail_value, LV_ALIGN_CENTER, 0, 15);

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
            zoom_detail = NULL;
        }
        zoom_card_index = -1;
    }

    static void hide_network_editor() {
        if (wifi_edit_overlay) {
            lv_obj_del(wifi_edit_overlay);
            wifi_edit_overlay = NULL;
            editor_panel = NULL;
            ssid_editor = NULL;
            password_editor = NULL;
            editing_index = -1;
            
            // Hide the global keyboard just in case
            if (keyboard) {
                lv_keyboard_set_textarea(keyboard, NULL);
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    static void refresh_settings_list() {
        if (!settings_list) {
            return;
        }

        lv_obj_clean(settings_list);
        std::vector<NetworkManager::WifiCredential> networks = NetworkManager::loadSavedNetworks();

        if (networks.empty()) {
            lv_obj_t *empty_label = lv_label_create(settings_list);
            lv_label_set_text(empty_label, "No saved networks");
            lv_obj_center(empty_label);
            return;
        }

        for (size_t i = 0; i < networks.size(); ++i) {
            lv_obj_t *row = lv_obj_create(settings_list);
            lv_obj_set_size(row, lv_pct(100), 36);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *row_btn = lv_btn_create(row);
            lv_obj_set_size(row_btn, lv_pct(78), lv_pct(100));
            lv_obj_set_style_radius(row_btn, 6, 0);
            lv_obj_set_style_bg_color(row_btn, lv_color_hex(0x2D3E50), 0);
            lv_obj_set_user_data(row_btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
            lv_obj_add_event_cb(row_btn, network_row_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_t *row_label = lv_label_create(row_btn);
            lv_label_set_text_fmt(row_label, "%s", networks[i].ssid.c_str());
            lv_obj_center(row_label);

            lv_obj_t *delete_btn = lv_btn_create(row);
            lv_obj_set_size(delete_btn, lv_pct(18), lv_pct(100));
            lv_obj_set_style_radius(delete_btn, 6, 0);
            lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xA63A3A), 0);
            lv_obj_set_user_data(delete_btn, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
            lv_obj_add_event_cb(delete_btn, network_delete_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_t *delete_label = lv_label_create(delete_btn);
            lv_label_set_text(delete_label, "Del");
            lv_obj_center(delete_label);
        }
    }

    static void textarea_event_cb(lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t * ta = lv_event_get_target(e);
        if (code == LV_EVENT_FOCUSED) {
            if (keyboard) {
                lv_keyboard_set_textarea(keyboard, ta);
                lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
        } else if (code == LV_EVENT_DEFOCUSED) {
            if (keyboard) {
                lv_keyboard_set_textarea(keyboard, NULL);
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }


    static void show_network_editor(int index) {
        hide_network_editor();
        if (!settings_content) {
            return;
        }

        editing_index = index;

        wifi_edit_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(wifi_edit_overlay, screen_width, screen_height);
        lv_obj_set_style_bg_color(wifi_edit_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(wifi_edit_overlay, LV_OPA_70, 0); // No background overlay, just a container
        lv_obj_set_style_border_width(wifi_edit_overlay, 0, 0);
        lv_obj_set_style_radius(wifi_edit_overlay, 0, 0);
        lv_obj_set_scrollbar_mode(wifi_edit_overlay, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(wifi_edit_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(wifi_edit_overlay, network_cancel_event_cb, LV_EVENT_CLICKED, NULL);

        editor_panel = lv_obj_create(wifi_edit_overlay); // Make it fullscreen over settings
        lv_obj_set_size(editor_panel, 190, 185);
        lv_obj_align(editor_panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(editor_panel, lv_color_hex(0xFFB19A), 0);
        lv_obj_set_style_bg_opa(editor_panel, LV_OPA_80, 0);
        lv_obj_set_style_pad_all(editor_panel, 10, 0);
        lv_obj_set_scrollbar_mode(editor_panel, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *title = lv_label_create(editor_panel);
        lv_label_set_text(title, index >= 0 ? "Edit Network" : "Add Network");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

        ssid_editor = lv_textarea_create(editor_panel);
        lv_textarea_set_one_line(ssid_editor, true);
        lv_textarea_set_placeholder_text(ssid_editor, "SSID");
        lv_obj_set_size(ssid_editor, lv_pct(100), 32);
        lv_obj_align(ssid_editor, LV_ALIGN_TOP_LEFT, 0, 25);
        lv_obj_add_event_cb(ssid_editor, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(ssid_editor, LV_SCROLLBAR_MODE_OFF);

        password_editor = lv_textarea_create(editor_panel);
        lv_textarea_set_one_line(password_editor, true);
        lv_textarea_set_password_mode(password_editor, true);
        lv_textarea_set_placeholder_text(password_editor, "Password");
        lv_obj_set_size(password_editor, lv_pct(100), 32);
        lv_obj_align(password_editor, LV_ALIGN_TOP_LEFT, 0, 62);
        lv_obj_add_event_cb(password_editor, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(password_editor, LV_SCROLLBAR_MODE_OFF);

        if (index >= 0) {
            std::vector<NetworkManager::WifiCredential> networks = NetworkManager::loadSavedNetworks();
            if (index < static_cast<int>(networks.size())) {
                lv_textarea_set_text(ssid_editor, networks[index].ssid.c_str());
                lv_textarea_set_text(password_editor, networks[index].password.c_str());
            }
        }

        lv_obj_t *confirm_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(confirm_btn, 80, 28);
        lv_obj_align(confirm_btn, LV_ALIGN_TOP_LEFT, 0, 99);
        lv_obj_add_event_cb(confirm_btn, network_confirm_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *confirm_label = lv_label_create(confirm_btn);
        lv_label_set_text(confirm_label, "Save");
        lv_obj_center(confirm_label);

        lv_obj_t *cancel_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(cancel_btn, 80, 28);
        lv_obj_align(cancel_btn, LV_ALIGN_TOP_RIGHT, 0, 99);
        lv_obj_add_event_cb(cancel_btn, network_cancel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_center(cancel_label);

        lv_obj_t *connect_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(connect_btn, 100, 28);
        lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_event_cb(connect_btn, network_connect_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *connect_label = lv_label_create(connect_btn);
        lv_label_set_text(connect_label, "Connect");
        lv_obj_center(connect_label);
    }

    static void settings_close_event_cb(lv_event_t * e) {
        (void)e;
        if (settings_content) {
            // lv_obj_del(settings_overlay);
            // settings_overlay = NULL;
            lv_obj_del(settings_content);
            settings_content = NULL;
            settings_list = NULL;
            metrics_ip_ta = NULL;
            metrics_path_ta = NULL;
            
            // Hide the global keyboard just in case
            if (keyboard) {
                lv_keyboard_set_textarea(keyboard, NULL);
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            }
            
            hide_network_editor();
        }
    }

    static void settings_add_event_cb(lv_event_t * e) {
        (void)e;
        show_network_editor(-1);
    }

    static void network_row_event_cb(lv_event_t * e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
        }

        lv_obj_t *target = lv_event_get_target(e);
        int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
        show_network_editor(index);
    }

    static void network_delete_event_cb(lv_event_t * e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
        }

        lv_obj_t *target = lv_event_get_target(e);
        int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
        NetworkManager::deleteNetwork(index);
        refresh_settings_list();
    }

    static void network_confirm_event_cb(lv_event_t * e) {
        (void)e;
        if (!ssid_editor || !password_editor) {
            return;
        }

        String ssid = lv_textarea_get_text(ssid_editor);
        String password = lv_textarea_get_text(password_editor);
        if (ssid.length() == 0) {
            return;
        }

        NetworkManager::addOrUpdateNetwork(ssid, password, editing_index);
        refresh_settings_list();
        hide_network_editor();
    }

    static void network_cancel_event_cb(lv_event_t * e) {
        (void)e;
        hide_network_editor();
    }

    static void network_connect_event_cb(lv_event_t * e) {
        (void)e;
        if (!ssid_editor || !password_editor) {
            return;
        }

        String ssid = lv_textarea_get_text(ssid_editor);
        String password = lv_textarea_get_text(password_editor);
        if (ssid.length() == 0) {
            return;
        }

        // Save it first
        NetworkManager::addOrUpdateNetwork(ssid, password, editing_index);
        
        // Show a message box
        static const char * btns[] = {""};
        lv_obj_t * mbox = lv_msgbox_create(editor_panel, "Connecting", "Connecting to WiFi...", btns, true);
        lv_obj_center(mbox);

        // Call the connection logic
        bool success = NetworkManager::connectToNetwork(ssid, password);
        
        if (success) {
            lv_msgbox_close(mbox);
            refresh_settings_list();
            hide_network_editor();
        } else {
            lv_msgbox_close(mbox);
            lv_obj_t * err_mbox = lv_msgbox_create(editor_panel, "Error", "Failed to connect.", btns, true);
            lv_obj_center(err_mbox);
        }
    }

    void showSettingsScreen() {
        if (settings_content) {
            return;
        }

        // settings_overlay = lv_obj_create(lv_scr_act());
        // lv_obj_set_size(settings_overlay, screen_width, screen_height);
        // lv_obj_set_style_bg_opa(settings_overlay, LV_OPA_TRANSP, 0); // No background overlay, just a container
        // lv_obj_set_style_border_width(settings_overlay, 0, 0);
        // lv_obj_set_scrollbar_mode(settings_overlay, LV_SCROLLBAR_MODE_OFF);

        settings_content = lv_obj_create(lv_scr_act());
        lv_obj_set_size(settings_content, screen_width, screen_height);
        lv_obj_align(settings_content, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(settings_content, 0, 0); 
        lv_obj_set_style_bg_color(settings_content, lv_color_hex(0x63A986), 0);
        lv_obj_set_style_bg_opa(settings_content, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(settings_content, 0, 0);
        lv_obj_set_style_pad_all(settings_content, 10, 0);
        lv_obj_set_style_border_width(settings_content, 0, 0);
        lv_obj_set_scrollbar_mode(settings_content, LV_SCROLLBAR_MODE_AUTO);

        lv_obj_t *title = lv_label_create(settings_content);
        lv_label_set_text(title, "Settings");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *close_btn = lv_btn_create(settings_content);
        lv_obj_set_size(close_btn, 30, 24);
        lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_event_cb(close_btn, settings_close_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "X");
        lv_obj_center(close_label);

        // Touch Calibration Trigger
        lv_obj_t *calib_btn = lv_btn_create(settings_content);
        lv_obj_set_size(calib_btn, 120, 28);
        lv_obj_align(calib_btn, LV_ALIGN_TOP_LEFT, 0, 240); // Below list
        lv_obj_add_event_cb(calib_btn, [](lv_event_t * e) {
            triggerTouchCalibration(); // From Touch_Calibrator.h
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *calib_label = lv_label_create(calib_btn);
        lv_label_set_text(calib_label, "Calibrate Touch");
        lv_obj_center(calib_label);

        lv_obj_t *add_btn = lv_btn_create(settings_content);
        lv_obj_set_size(add_btn, 90, 28);
        lv_obj_align(add_btn, LV_ALIGN_TOP_RIGHT, 0, 240); // Below list
        lv_obj_add_event_cb(add_btn, settings_add_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *add_label = lv_label_create(add_btn);
        lv_label_set_text(add_label, "Add WiFi");
        lv_obj_center(add_label);

        // Wi-Fi Label
        lv_obj_t *list_label = lv_label_create(settings_content);
        lv_label_set_text(list_label, "Saved Networks:");
        lv_obj_align(list_label, LV_ALIGN_TOP_LEFT, 0, 90);

        settings_list = lv_obj_create(settings_content);
        lv_obj_set_size(settings_list, lv_pct(100), 120);
        lv_obj_align(settings_list, LV_ALIGN_TOP_MID, 0, 110);
        lv_obj_set_flex_flow(settings_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(settings_list, 4, 0);
        lv_obj_set_style_pad_all(settings_list, 4, 0);
        lv_obj_set_style_bg_opa(settings_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(settings_list, 1, 0);
        lv_obj_set_style_border_color(settings_list, lv_color_hex(0x555555), 0);
        lv_obj_set_scrollbar_mode(settings_list, LV_SCROLLBAR_MODE_AUTO);

        refresh_settings_list();

        // Metrics inputs
        lv_obj_t *metrics_label = lv_label_create(settings_content);
        lv_label_set_text(metrics_label, "Metrics Config:");
        lv_obj_align(metrics_label, LV_ALIGN_TOP_LEFT, 0, 30);

        metrics_ip_ta = lv_textarea_create(settings_content);
        lv_textarea_set_one_line(metrics_ip_ta, true);
        lv_textarea_set_placeholder_text(metrics_ip_ta, "IP");
        lv_obj_set_size(metrics_ip_ta, 120, 32);
        lv_obj_align(metrics_ip_ta, LV_ALIGN_TOP_LEFT, 0, 50);
        lv_textarea_set_text(metrics_ip_ta, NetworkManager::getMetricsIP().c_str());
        lv_obj_add_event_cb(metrics_ip_ta, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(metrics_ip_ta, LV_SCROLLBAR_MODE_OFF);

        metrics_path_ta = lv_textarea_create(settings_content);
        lv_textarea_set_one_line(metrics_path_ta, true);
        lv_textarea_set_placeholder_text(metrics_path_ta, "Path");
        lv_obj_set_size(metrics_path_ta, 90, 32);
        lv_obj_align(metrics_path_ta, LV_ALIGN_TOP_LEFT, 130, 50);
        lv_textarea_set_text(metrics_path_ta, NetworkManager::getMetricsPath().c_str());
        lv_obj_add_event_cb(metrics_path_ta, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(metrics_path_ta, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *m_save_btn = lv_btn_create(settings_content);
        lv_obj_set_size(m_save_btn, 65, 32);
        lv_obj_align(m_save_btn, LV_ALIGN_TOP_LEFT, 230, 50);
        lv_obj_add_event_cb(m_save_btn, [](lv_event_t * e) {
            String ip = lv_textarea_get_text(metrics_ip_ta);
            String path = lv_textarea_get_text(metrics_path_ta);
            if (ip.length() > 0 && path.length() > 0) {
                NetworkManager::setMetricsConfig(ip, path);
                
                static const char * btns[] = {""};
                lv_obj_t * mbox = lv_msgbox_create(settings_content, "Saved", "Metrics config saved.", btns, true);
                lv_obj_center(mbox);
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *m_save_lbl = lv_label_create(m_save_btn);
        lv_label_set_text(m_save_lbl, "Save");
        lv_obj_center(m_save_lbl);
    }

    static void show_brightness_slider() {
        if (brightness_overlay) return;

        brightness_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(brightness_overlay, screen_width, screen_height);
        lv_obj_set_style_bg_color(brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(brightness_overlay, LV_OPA_40, 0);
        lv_obj_set_scrollbar_mode(brightness_overlay, LV_SCROLLBAR_MODE_OFF);        
        lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t * panel = lv_obj_create(brightness_overlay);
        lv_obj_set_size(panel, 200, 80);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);        

        lv_obj_t * slider = lv_slider_create(panel);
        lv_obj_set_size(slider, 160, 20);
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, -10);
        lv_slider_set_range(slider, 10, 255);
        
        // Read current
        Preferences prefs;
        prefs.begin("settings", true);
        initial_brightness_val = prefs.getInt("brightness", 128);
        prefs.end();
        lv_slider_set_value(slider, initial_brightness_val, LV_ANIM_OFF);

        // Click on overlay to cancel
        lv_obj_add_event_cb(brightness_overlay, [](lv_event_t * e) {
            lv_obj_t * target = lv_event_get_target(e);
            lv_obj_t * current = lv_event_get_current_target(e);
            if (target == current) { // Ensure they didn't click the panel or slider
                tft.setBrightness(initial_brightness_val); // Revert hardware
                lv_obj_del(brightness_overlay);
                brightness_overlay = NULL;
            }
        }, LV_EVENT_CLICKED, NULL);

        // Callback to change brightness instantly on slide, NO NVS SAVING HERE
        lv_obj_add_event_cb(slider, [](lv_event_t * e) {
            lv_obj_t * slider_obj = lv_event_get_target(e);
            int v = lv_slider_get_value(slider_obj);
            tft.setBrightness(v);
        }, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t * close_btn = lv_btn_create(panel);
        lv_obj_set_size(close_btn, 60, 24);
        lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, 5);
        
        // Let's pass the slider object to the OK button as user_data so we can read its value
        lv_obj_add_event_cb(close_btn, [](lv_event_t * e) {
            lv_obj_t * slider_obj = (lv_obj_t *)lv_event_get_user_data(e);
            int v = lv_slider_get_value(slider_obj);
            
            Preferences p;
            p.begin("settings", false);
            p.putInt("brightness", v);
            p.end();

            lv_obj_del(brightness_overlay);
            brightness_overlay = NULL;
        }, LV_EVENT_CLICKED, slider);

        lv_obj_t * close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "OK");
        lv_obj_center(close_label);
    }

    void init(uint16_t tft_width, uint16_t tft_height) {
        screen_width = tft_width;
        screen_height = tft_height;

        // Create the global persistent keyboard on the top layer
        keyboard = lv_keyboard_create(lv_layer_top());
        lv_obj_set_size(keyboard, screen_width, screen_height / 2 + 40);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(keyboard, 191, LV_PART_MAIN); // 75% opacity
        lv_obj_set_style_bg_opa(keyboard, 191, LV_PART_ITEMS);
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

        touch_cursor = lv_obj_create(lv_layer_sys());
        lv_obj_set_size(touch_cursor, 20, 20);
        lv_obj_set_style_radius(touch_cursor, 10, 0);
        lv_obj_set_style_bg_color(touch_cursor, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_bg_opa(touch_cursor, LV_OPA_80, 0);
        lv_obj_add_flag(touch_cursor, LV_OBJ_FLAG_HIDDEN);

        auto create_card = [](const char *title, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, uint32_t bg_color, lv_obj_t **out_value){
            const int M = 8;
            int card_w = (screen_width - 3 * M) / 2;
            int card_h = (screen_height - 3 * M) / 2 * 0.80f;

            lv_obj_t *c = lv_obj_create(lv_scr_act());
            lv_obj_set_size(c, card_w, card_h);
            lv_obj_set_align(c, align);

            lv_obj_set_style_pad_all(c, 10, 0);
            lv_obj_set_style_radius(c, 8, 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(0x444444), 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(bg_color), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
            lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
            
            lv_obj_t *t = lv_label_create(c);
            lv_label_set_text(t, title);
            lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
            lv_obj_align(t, LV_ALIGN_TOP_LEFT, 10, 6);

            lv_obj_t *v = lv_label_create(c);
            lv_label_set_text(v, "--");
            lv_obj_set_style_text_font(v, &lv_font_montserrat_22, 0);
            lv_obj_align(v, LV_ALIGN_CENTER, 0, 10);

            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, card_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_set_pos(c, x_ofs, y_ofs);

            if (out_value) *out_value = v;
            return c;
        };

        card_obj[0] = create_card("Voltage (V)", LV_ALIGN_TOP_LEFT, 7, 30, 0x4A90E2, &card_value[0]);
        card_obj[1] = create_card("Current (A)", LV_ALIGN_TOP_RIGHT, -7, 30, 0xE74C3C, &card_value[1]);
        card_obj[2] = create_card("Energy (kWh)", LV_ALIGN_BOTTOM_LEFT, 7, -30, 0x2ECC71, &card_value[2]);
        card_obj[3] = create_card("Power (W)", LV_ALIGN_BOTTOM_RIGHT, -7, -30, 0xF39C12, &card_value[3]);

        // HUD: Top layer for hovering buttons
        lv_obj_t * top_layer = lv_scr_act();

        // Settings Button
        lv_obj_t *settings_button = lv_btn_create(top_layer);
        lv_obj_set_size(settings_button, 40, 40);
        lv_obj_set_style_radius(settings_button, 20, 0); // Circle
        lv_obj_align(settings_button, LV_ALIGN_TOP_RIGHT, -5, 5);
        lv_obj_add_event_cb(settings_button, [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                showSettingsScreen();
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *settings_label = lv_label_create(settings_button);
        lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
        lv_obj_center(settings_label);

        // Brightness Button
        lv_obj_t *brightness_button = lv_btn_create(top_layer);
        lv_obj_set_size(brightness_button, 40, 40);
        lv_obj_set_style_radius(brightness_button, 20, 0); // Circle
        lv_obj_align(brightness_button, LV_ALIGN_TOP_RIGHT, -50, 5);
        lv_obj_add_event_cb(brightness_button, [](lv_event_t *e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                show_brightness_slider();
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *brightness_label = lv_label_create(brightness_button);
        lv_label_set_text(brightness_label, LV_SYMBOL_EYE_OPEN); // Using eye symbol as brightness approx
        lv_obj_center(brightness_label);

        // Fetch Error Icon
        fetch_error_icon = lv_label_create(top_layer);
        lv_label_set_text(fetch_error_icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(fetch_error_icon, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_text_font(fetch_error_icon, &lv_font_montserrat_48, 0);
        lv_obj_align(fetch_error_icon, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(fetch_error_icon, LV_OBJ_FLAG_HIDDEN); // Hidden by default
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

        if (zoom_detail && zoom_card_index >= 0) {
            lv_obj_t *detail_val = (lv_obj_t*)lv_obj_get_user_data(zoom_detail);
            if (detail_val) {
                lv_label_set_text(detail_val, lv_label_get_text(card_value[zoom_card_index]));
            }
        }
    }

    void setFetchError(bool error) {
        if (!fetch_error_icon) return;
        if (error) {
            lv_obj_clear_flag(fetch_error_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(fetch_error_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
