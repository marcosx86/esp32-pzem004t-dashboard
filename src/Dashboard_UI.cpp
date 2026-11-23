#include "Dashboard_UI.h"
#include "Touch_Calibrator.h"
#include "Display_LGFX.h"
#include <Preferences.h>
#include <cmath>

namespace DashboardUI {
    // Global touch cursor pointer
    lv_obj_t *touch_cursor = NULL;

    // Theme & runtime settings state
    static NetworkManager::AppSettings app_settings;
    static TouchResetCallback touch_reset_cb = NULL;

    // Dashboard cards
    static lv_obj_t *card_obj[4] = {NULL, NULL, NULL, NULL};
    static lv_obj_t *card_title[4] = {NULL, NULL, NULL, NULL};
    static lv_obj_t *card_value[4] = {NULL, NULL, NULL, NULL};
    static lv_obj_t *card_sub[4] = {NULL, NULL, NULL, NULL};

    // Floating top-50px HUD elements
    static lv_obj_t *hud_status_btn = NULL;
    static lv_obj_t *hud_freshness_dot = NULL;
    static lv_obj_t *hud_wifi_label = NULL;
    static lv_obj_t *hud_ambient_pill = NULL;
    static lv_obj_t *hud_ambient_label = NULL;

    // Diagnostics popup
    static lv_obj_t *diag_overlay = NULL;

    // Zoom modal
    static int zoom_card_index = -1;
    static lv_obj_t *zoom_overlay = NULL;
    static lv_obj_t *zoom_detail = NULL;

    // Settings modal & tracked themed children
    static lv_obj_t *settings_content = NULL;
    static lv_obj_t *settings_list = NULL;
    static lv_obj_t *settings_title_label = NULL;
    static lv_obj_t *metrics_section_label = NULL;
    static lv_obj_t *saved_nets_label = NULL;
    static lv_obj_t *appearance_section_label = NULL;
    static lv_obj_t *theme_row_label = NULL;
    static lv_obj_t *cursor_row_label = NULL;

    // Wi-Fi Editor UI
    static lv_obj_t *editor_panel = NULL;
    static lv_obj_t *wifi_edit_overlay = NULL;
    static lv_obj_t *editor_title_label = NULL;
    static lv_obj_t *ssid_editor = NULL;
    static lv_obj_t *password_editor = NULL;
    static lv_obj_t *keyboard = NULL;
    static int editing_index = -1;

    // Async Wi-Fi Scan UI
    static lv_obj_t *scan_overlay = NULL;
    static lv_obj_t *scan_list = NULL;
    static lv_obj_t *scan_spinner = NULL;
    static lv_timer_t *scan_poll_timer = NULL;

    // Non-blocking Wi-Fi Connect UI
    static lv_obj_t *connect_overlay = NULL;
    static lv_timer_t *connect_poll_timer = NULL;

    // Metrics inputs
    static lv_obj_t *metrics_ip_ta = NULL;
    static lv_obj_t *metrics_path_ta = NULL;

    // Brightness UI
    static lv_obj_t *brightness_overlay = NULL;
    static int initial_brightness_val = 128;

    static uint16_t screen_width = 320;
    static uint16_t screen_height = 240;

    // Custom 4x4 IP / Numeric Keymap (16 buttons matching ip_kb_ctrl length)
    static const char * ip_kb_map[] = {
        "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
        "4", "5", "6", ":", "\n",
        "7", "8", "9", ".", "\n",
        "abc", "0", LV_SYMBOL_KEYBOARD, LV_SYMBOL_OK, ""
    };

    static const lv_btnmatrix_ctrl_t ip_kb_ctrl[] = {
        1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2,
        1, 1, 1, 2,
        1, 1, 1, 2,
        LV_KEYBOARD_CTRL_BTN_FLAGS | 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
    };

    // Forward declarations
    static void zoom_in(int card_idx);
    static void zoom_out();
    static void apply_theme(int theme_id);
    static void refresh_settings_list();
    static void close_connect_overlay();
    static void close_scan_modal();
    static void hide_network_editor();
    static void show_network_editor(int index, const char *prefill_ssid = NULL);
    static void show_wifi_scan_modal();
    static void show_diagnostics_modal();
    static void close_diagnostics_modal();
    static void hide_keyboard_and_restore();

    static void tag_textarea_kind(lv_obj_t *ta, InputFieldKind kind) {
        lv_obj_set_user_data(ta, reinterpret_cast<void*>(static_cast<intptr_t>(kind)));
    }

    static bool is_child_of(const lv_obj_t *obj, const lv_obj_t *ancestor) {
        const lv_obj_t *cur = obj;
        while (cur) {
            if (cur == ancestor) {
                return true;
            }
            cur = lv_obj_get_parent(cur);
        }
        return false;
    }

    static void hide_keyboard_and_restore() {
        if (keyboard) {
            lv_obj_t *prev_ta = lv_keyboard_get_textarea(keyboard);
            if (prev_ta) {
                lv_obj_clear_state(prev_ta, LV_STATE_FOCUSED);
            }
            lv_keyboard_set_textarea(keyboard, NULL);
            lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        if (settings_content) {
            lv_obj_set_style_pad_bottom(settings_content, 10, 0);
        }
        if (editor_panel) {
            lv_obj_align(editor_panel, LV_ALIGN_CENTER, 0, 0);
        }
    }

    static void keyboard_event_cb(lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            hide_keyboard_and_restore();
        }
    }

    static void textarea_event_cb(lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *ta = lv_event_get_target(e);

        if (code == LV_EVENT_FOCUSED) {
            if (!keyboard) return;
            InputFieldKind kind = static_cast<InputFieldKind>(
                reinterpret_cast<intptr_t>(lv_obj_get_user_data(ta)));
            lv_keyboard_set_mode(
                keyboard,
                (kind == InputFieldKind::NUMERIC_IP) ? LV_KEYBOARD_MODE_USER_1 : LV_KEYBOARD_MODE_TEXT_LOWER);
            lv_keyboard_set_textarea(keyboard, ta);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

            if (editor_panel && is_child_of(ta, editor_panel)) {
                // Lift editor panel into the top 80px band above the 160px keyboard
                lv_obj_align(editor_panel, LV_ALIGN_TOP_MID, 0, 2);
            } else if (settings_content && is_child_of(ta, settings_content)) {
                lv_obj_set_style_pad_bottom(settings_content, 170, 0);
                lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
            }
        } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
            if (keyboard && lv_keyboard_get_textarea(keyboard) == ta) {
                hide_keyboard_and_restore();
            }
        }
    }

    static void apply_theme(int theme_id) {
        app_settings.theme = normalizeThemeId(theme_id);
        const UiThemePalette &p = getThemePalette(app_settings.theme);

        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(p.screen_bg), 0);
        lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

        for (int i = 0; i < 4; ++i) {
            if (card_obj[i]) {
                lv_obj_set_style_bg_color(card_obj[i], lv_color_hex(p.card_bg[i]), 0);
                lv_obj_set_style_border_color(card_obj[i], lv_color_hex(p.card_border[i]), 0);
                lv_obj_set_style_border_width(card_obj[i], p.card_border_width, 0);
            }
            if (card_title[i]) {
                lv_obj_set_style_text_color(card_title[i], lv_color_hex(p.card_title[i]), 0);
            }
            if (card_value[i]) {
                lv_obj_set_style_text_color(card_value[i], lv_color_hex(p.card_value), 0);
            }
            if (card_sub[i]) {
                lv_obj_set_style_text_color(card_sub[i], lv_color_hex(p.card_sub), 0);
            }
        }

        if (hud_status_btn) {
            lv_obj_set_style_bg_color(hud_status_btn, lv_color_hex(p.hud_pill_bg), 0);
        }
        if (hud_wifi_label) {
            lv_obj_set_style_text_color(hud_wifi_label, lv_color_hex(p.hud_pill_text), 0);
        }
        if (hud_ambient_pill) {
            lv_obj_set_style_bg_color(hud_ambient_pill, lv_color_hex(p.hud_pill_bg), 0);
        }
        if (hud_ambient_label) {
            lv_obj_set_style_text_color(hud_ambient_label, lv_color_hex(p.hud_pill_text), 0);
        }

        if (zoom_detail && zoom_card_index >= 0 && zoom_card_index < 4) {
            lv_obj_set_style_bg_color(zoom_detail, lv_color_hex(p.card_bg[zoom_card_index]), 0);
            lv_obj_set_style_border_color(zoom_detail, lv_color_hex(p.card_border[zoom_card_index]), 0);
        }

        if (settings_content) {
            lv_obj_set_style_bg_color(settings_content, lv_color_hex(p.settings_bg), 0);
            lv_obj_t *text_labels[] = {
                settings_title_label,
                metrics_section_label,
                saved_nets_label,
                appearance_section_label,
                theme_row_label,
                cursor_row_label
            };
            for (lv_obj_t *lbl : text_labels) {
                if (lbl) {
                    lv_obj_set_style_text_color(lbl, lv_color_hex(p.settings_text), 0);
                }
            }
        }
        if (settings_list) {
            lv_obj_set_style_border_color(settings_list, lv_color_hex(p.list_border), 0);
        }
        if (editor_panel) {
            lv_obj_set_style_bg_color(editor_panel, lv_color_hex(p.editor_bg), 0);
            if (editor_title_label) {
                lv_obj_set_style_text_color(editor_title_label, lv_color_hex(p.settings_text), 0);
            }
        }
    }

    static void card_event_cb(lv_event_t * e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
        }
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

    static void zoom_in(int card_idx) {
        if (zoom_card_index != -1) zoom_out();

        zoom_card_index = card_idx;
        const UiThemePalette &p = getThemePalette(app_settings.theme);

        zoom_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(zoom_overlay, screen_width, screen_height);
        lv_obj_set_align(zoom_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(zoom_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(zoom_overlay, LV_OPA_50, 0);
        lv_obj_set_style_border_width(zoom_overlay, 0, 0);
        lv_obj_set_style_radius(zoom_overlay, 0, 0);
        lv_obj_add_flag(zoom_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(zoom_overlay, [](lv_event_t * e) {
            if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
                zoom_out();
            }
        }, LV_EVENT_CLICKED, NULL);

        zoom_detail = lv_obj_create(zoom_overlay);
        lv_obj_set_size(zoom_detail, screen_width - 40, screen_height - 110);
        lv_obj_set_align(zoom_detail, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(zoom_detail, 12, 0);
        lv_obj_set_style_border_width(zoom_detail, 3, 0);
        lv_obj_set_style_border_color(zoom_detail, lv_color_hex(p.card_border[card_idx]), 0);
        lv_obj_set_style_pad_all(zoom_detail, 10, 0);
        lv_obj_set_scrollbar_mode(zoom_detail, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_color(zoom_detail, lv_color_hex(p.card_bg[card_idx]), 0);
        lv_obj_set_style_bg_opa(zoom_detail, LV_OPA_COVER, 0);

        lv_obj_t *title = lv_label_create(zoom_detail);
        static const char* titles[] = {"Voltage (V)", "Current (A)", "Energy (kWh)", "Power (W)"};
        lv_label_set_text(title, titles[card_idx]);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(p.card_title[card_idx]), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);

        lv_obj_t *detail_value = lv_label_create(zoom_detail);
        const char *current_val = lv_label_get_text(card_value[card_idx]);
        lv_label_set_text(detail_value, current_val);
        lv_obj_set_style_text_font(detail_value, &lv_font_montserrat_44, 0);
        lv_obj_set_style_text_color(detail_value, lv_color_hex(p.card_value), 0);
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

    static void close_diagnostics_modal() {
        if (diag_overlay) {
            lv_obj_del(diag_overlay);
            diag_overlay = NULL;
        }
    }

    static void show_diagnostics_modal() {
        close_diagnostics_modal();
        NetworkManager::NetworkDiagnostics diag = NetworkManager::getDiagnostics();
        const UiThemePalette &p = getThemePalette(app_settings.theme);

        diag_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(diag_overlay, screen_width, screen_height);
        lv_obj_set_align(diag_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(diag_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(diag_overlay, LV_OPA_60, 0);
        lv_obj_set_style_border_width(diag_overlay, 0, 0);
        lv_obj_set_style_radius(diag_overlay, 0, 0);
        lv_obj_set_scrollbar_mode(diag_overlay, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(diag_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(diag_overlay, [](lv_event_t * e) {
            if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
                close_diagnostics_modal();
            }
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t *panel = lv_obj_create(diag_overlay);
        lv_obj_set_size(panel, 250, 175);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(p.hud_pill_bg), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x4A90E2), 0);
        lv_obj_set_style_border_width(panel, 2, 0);
        lv_obj_set_style_radius(panel, 10, 0);
        lv_obj_set_style_pad_all(panel, 10, 0);
        lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "Link Diagnostics");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *body = lv_label_create(panel);
        lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(body, lv_color_hex(0xDCE4EE), 0);
        lv_obj_set_width(body, 226);

        uint32_t now_ms = millis();
        HudFreshness fresh = computeHudFreshness(
            diag.wifi_connected, diag.has_valid_metrics, diag.fetch_failed, diag.last_success_ms, now_ms);
        const char *state_str = (fresh == HudFreshness::LIVE) ? "LIVE"
                              : (fresh == HudFreshness::STALE ? "STALE" : "ERROR");
        uint32_t age_sec = diag.has_valid_metrics
            ? static_cast<uint32_t>(now_ms - diag.last_success_ms) / 1000u
            : 0u;

        if (diag.wifi_connected) {
            lv_label_set_text_fmt(
                body,
                "Wi-Fi: %s (%ld dBm)\nIP: %s\nMetrics: %s (%lus)\nHTTP code: %d",
                diag.ssid.c_str(),
                static_cast<long>(diag.rssi_dbm),
                diag.ip.c_str(),
                state_str,
                static_cast<unsigned long>(age_sec),
                diag.last_http_code);
        } else {
            lv_label_set_text_fmt(
                body,
                "Wi-Fi: Disconnected\nMetrics: %s\nHTTP code: %d",
                state_str,
                diag.last_http_code);
        }
        lv_obj_align(body, LV_ALIGN_TOP_LEFT, 0, 28);

        lv_obj_t *ok_btn = lv_btn_create(panel);
        lv_obj_set_size(ok_btn, 70, 28);
        lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_event_cb(ok_btn, [](lv_event_t * e) {
            (void)e;
            close_diagnostics_modal();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *ok_lbl = lv_label_create(ok_btn);
        lv_label_set_text(ok_lbl, "OK");
        lv_obj_center(ok_lbl);
    }

    static void close_connect_overlay() {
        if (connect_poll_timer) {
            lv_timer_del(connect_poll_timer);
            connect_poll_timer = NULL;
        }
        if (connect_overlay) {
            lv_obj_del(connect_overlay);
            connect_overlay = NULL;
        }
    }

    static void close_scan_modal() {
        if (scan_poll_timer) {
            lv_timer_del(scan_poll_timer);
            scan_poll_timer = NULL;
        }
        if (scan_overlay) {
            lv_obj_del(scan_overlay);
            scan_overlay = NULL;
            scan_list = NULL;
            scan_spinner = NULL;
        }
    }

    static void hide_network_editor() {
        close_connect_overlay();
        if (wifi_edit_overlay) {
            lv_obj_del(wifi_edit_overlay);
            wifi_edit_overlay = NULL;
            editor_panel = NULL;
            editor_title_label = NULL;
            ssid_editor = NULL;
            password_editor = NULL;
            editing_index = -1;
            hide_keyboard_and_restore();
        }
    }

    static void network_row_event_cb(lv_event_t * e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
            return;
        }
        lv_obj_t *target = lv_event_get_target(e);
        int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
        show_network_editor(index, NULL);
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
            lv_obj_set_size(row, lv_pct(100), 34);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
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

    static void connect_poll_timer_cb(lv_timer_t * t) {
        (void)t;
        NetworkManager::ConnectState st = NetworkManager::getConnectState();
        if (st == NetworkManager::ConnectState::CONNECTED) {
            close_connect_overlay();
            NetworkManager::clearConnectState();
            refresh_settings_list();
            hide_network_editor();
        } else if (st == NetworkManager::ConnectState::FAILED) {
            close_connect_overlay();
            NetworkManager::clearConnectState();
            if (editor_panel) {
                static const char * btns[] = {""};
                lv_obj_t *err_mbox = lv_msgbox_create(editor_panel, "Error", "Failed to connect.", btns, true);
                lv_obj_center(err_mbox);
            }
        }
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

        hide_keyboard_and_restore();
        NetworkManager::addOrUpdateNetwork(ssid, password, editing_index);
        NetworkManager::requestConnect(ssid, password);

        close_connect_overlay();
        connect_overlay = lv_obj_create(wifi_edit_overlay ? wifi_edit_overlay : lv_scr_act());
        lv_obj_set_size(connect_overlay, screen_width, screen_height);
        lv_obj_set_align(connect_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(connect_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(connect_overlay, LV_OPA_60, 0);
        lv_obj_set_style_border_width(connect_overlay, 0, 0);
        lv_obj_set_style_radius(connect_overlay, 0, 0);
        lv_obj_set_scrollbar_mode(connect_overlay, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(connect_overlay, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *box = lv_obj_create(connect_overlay);
        lv_obj_set_size(box, 180, 110);
        lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *spin = lv_spinner_create(box, 1000, 60);
        lv_obj_set_size(spin, 42, 42);
        lv_obj_align(spin, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t *lbl = lv_label_create(box);
        lv_label_set_text(lbl, "Connecting...");
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

        connect_poll_timer = lv_timer_create(connect_poll_timer_cb, 200, NULL);
    }

    static void show_network_editor(int index, const char *prefill_ssid) {
        hide_network_editor();
        if (!settings_content) {
            return;
        }

        editing_index = index;
        const UiThemePalette &p = getThemePalette(app_settings.theme);

        wifi_edit_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(wifi_edit_overlay, screen_width, screen_height);
        lv_obj_set_style_bg_color(wifi_edit_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(wifi_edit_overlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(wifi_edit_overlay, 0, 0);
        lv_obj_set_style_radius(wifi_edit_overlay, 0, 0);
        lv_obj_set_scrollbar_mode(wifi_edit_overlay, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(wifi_edit_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(wifi_edit_overlay, [](lv_event_t * e) {
            if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
                hide_network_editor();
            }
        }, LV_EVENT_CLICKED, NULL);

        editor_panel = lv_obj_create(wifi_edit_overlay);
        lv_obj_set_size(editor_panel, 200, 185);
        lv_obj_align(editor_panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(editor_panel, lv_color_hex(p.editor_bg), 0);
        lv_obj_set_style_bg_opa(editor_panel, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(editor_panel, 10, 0);
        lv_obj_set_scrollbar_mode(editor_panel, LV_SCROLLBAR_MODE_OFF);

        editor_title_label = lv_label_create(editor_panel);
        lv_label_set_text(editor_title_label, index >= 0 ? "Edit Network" : "Add Network");
        lv_obj_set_style_text_color(editor_title_label, lv_color_hex(p.settings_text), 0);
        lv_obj_align(editor_title_label, LV_ALIGN_TOP_MID, 0, 0);

        ssid_editor = lv_textarea_create(editor_panel);
        lv_textarea_set_one_line(ssid_editor, true);
        lv_textarea_set_placeholder_text(ssid_editor, "SSID");
        lv_obj_set_size(ssid_editor, lv_pct(100), 32);
        lv_obj_align(ssid_editor, LV_ALIGN_TOP_LEFT, 0, 24);
        tag_textarea_kind(ssid_editor, InputFieldKind::TEXT);
        lv_obj_add_event_cb(ssid_editor, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(ssid_editor, LV_SCROLLBAR_MODE_OFF);

        password_editor = lv_textarea_create(editor_panel);
        lv_textarea_set_one_line(password_editor, true);
        lv_textarea_set_password_mode(password_editor, true);
        lv_textarea_set_placeholder_text(password_editor, "Password");
        lv_obj_set_size(password_editor, lv_pct(100), 32);
        lv_obj_align(password_editor, LV_ALIGN_TOP_LEFT, 0, 60);
        tag_textarea_kind(password_editor, InputFieldKind::TEXT);
        lv_obj_add_event_cb(password_editor, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(password_editor, LV_SCROLLBAR_MODE_OFF);

        if (index >= 0) {
            std::vector<NetworkManager::WifiCredential> networks = NetworkManager::loadSavedNetworks();
            if (index < static_cast<int>(networks.size())) {
                lv_textarea_set_text(ssid_editor, networks[index].ssid.c_str());
                lv_textarea_set_text(password_editor, networks[index].password.c_str());
            }
        } else if (prefill_ssid && prefill_ssid[0] != '\0') {
            lv_textarea_set_text(ssid_editor, prefill_ssid);
        }

        lv_obj_t *confirm_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(confirm_btn, 84, 28);
        lv_obj_align(confirm_btn, LV_ALIGN_TOP_LEFT, 0, 98);
        lv_obj_add_event_cb(confirm_btn, network_confirm_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *confirm_label = lv_label_create(confirm_btn);
        lv_label_set_text(confirm_label, "Save");
        lv_obj_center(confirm_label);

        lv_obj_t *cancel_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(cancel_btn, 84, 28);
        lv_obj_align(cancel_btn, LV_ALIGN_TOP_RIGHT, 0, 98);
        lv_obj_add_event_cb(cancel_btn, network_cancel_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_center(cancel_label);

        lv_obj_t *connect_btn = lv_btn_create(editor_panel);
        lv_obj_set_size(connect_btn, 110, 28);
        lv_obj_align(connect_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_event_cb(connect_btn, network_connect_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *connect_label = lv_label_create(connect_btn);
        lv_label_set_text(connect_label, "Connect");
        lv_obj_center(connect_label);
    }

    static void scan_poll_timer_cb(lv_timer_t * t) {
        (void)t;
        int status = NetworkManager::getWifiScanStatus();
        if (status == -1) {
            return; // WIFI_SCAN_RUNNING
        }

        if (scan_poll_timer) {
            lv_timer_del(scan_poll_timer);
            scan_poll_timer = NULL;
        }
        if (scan_spinner) {
            lv_obj_add_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
        }
        if (!scan_list) {
            return;
        }

        lv_obj_clean(scan_list);
        std::vector<NetworkManager::WifiScanEntry> entries = NetworkManager::getWifiScanResults();
        if (entries.empty()) {
            lv_obj_t *empty = lv_label_create(scan_list);
            lv_label_set_text(empty, "No networks found");
            lv_obj_center(empty);
            return;
        }

        for (size_t i = 0; i < entries.size(); ++i) {
            lv_obj_t *btn = lv_btn_create(scan_list);
            lv_obj_set_size(btn, lv_pct(100), 34);
            lv_obj_set_style_radius(btn, 6, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D3E50), 0);

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text_fmt(
                lbl,
                "%s (%ld dBm)%s",
                entries[i].ssid.c_str(),
                static_cast<long>(entries[i].rssi),
                entries[i].encrypted ? " *" : "");
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);

            char *ssid_dup = static_cast<char*>(lv_mem_alloc(entries[i].ssid.length() + 1));
            if (ssid_dup) {
                memcpy(ssid_dup, entries[i].ssid.c_str(), entries[i].ssid.length() + 1);
            }
            lv_obj_set_user_data(btn, ssid_dup);
            lv_obj_add_event_cb(btn, [](lv_event_t * e) {
                lv_event_code_t code = lv_event_get_code(e);
                lv_obj_t *target = lv_event_get_target(e);
                char *stored = static_cast<char*>(lv_obj_get_user_data(target));
                if (code == LV_EVENT_CLICKED) {
                    String chosen = stored ? String(stored) : String("");
                    close_scan_modal();
                    if (chosen.length() > 0) {
                        show_network_editor(-1, chosen.c_str());
                    }
                } else if (code == LV_EVENT_DELETE) {
                    if (stored) {
                        lv_mem_free(stored);
                        lv_obj_set_user_data(target, NULL);
                    }
                }
            }, LV_EVENT_ALL, NULL);
        }
    }

    static void show_wifi_scan_modal() {
        close_scan_modal();
        if (!settings_content) {
            return;
        }

        hide_keyboard_and_restore();
        NetworkManager::startWifiScan();
        const UiThemePalette &p = getThemePalette(app_settings.theme);

        scan_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(scan_overlay, screen_width, screen_height);
        lv_obj_set_align(scan_overlay, LV_ALIGN_CENTER);
        lv_obj_set_style_bg_color(scan_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scan_overlay, LV_OPA_70, 0);
        lv_obj_set_style_border_width(scan_overlay, 0, 0);
        lv_obj_set_style_radius(scan_overlay, 0, 0);
        lv_obj_set_scrollbar_mode(scan_overlay, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(scan_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(scan_overlay, [](lv_event_t * e) {
            if (lv_event_get_target(e) == lv_event_get_current_target(e)) {
                close_scan_modal();
            }
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t *panel = lv_obj_create(scan_overlay);
        lv_obj_set_size(panel, 270, 195);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(p.settings_bg), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(panel, 8, 0);
        lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "Scan WiFi");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(p.settings_text), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 4);

        lv_obj_t *close_btn = lv_btn_create(panel);
        lv_obj_set_size(close_btn, 32, 26);
        lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_add_event_cb(close_btn, [](lv_event_t * e) {
            (void)e;
            close_scan_modal();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *close_lbl = lv_label_create(close_btn);
        lv_label_set_text(close_lbl, "X");
        lv_obj_center(close_lbl);

        scan_list = lv_obj_create(panel);
        lv_obj_set_size(scan_list, lv_pct(100), 144);
        lv_obj_align(scan_list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_flex_flow(scan_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(scan_list, 4, 0);
        lv_obj_set_style_pad_all(scan_list, 4, 0);
        lv_obj_set_style_bg_opa(scan_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(scan_list, 1, 0);
        lv_obj_set_style_border_color(scan_list, lv_color_hex(p.list_border), 0);

        scan_spinner = lv_spinner_create(panel, 1000, 60);
        lv_obj_set_size(scan_spinner, 44, 44);
        lv_obj_align(scan_spinner, LV_ALIGN_CENTER, 0, 14);

        scan_poll_timer = lv_timer_create(scan_poll_timer_cb, 250, NULL);
    }

    static void settings_close_event_cb(lv_event_t * e) {
        (void)e;
        close_scan_modal();
        hide_network_editor();
        hide_keyboard_and_restore();

        if (settings_content) {
            lv_obj_del(settings_content);
            settings_content = NULL;
            settings_list = NULL;
            settings_title_label = NULL;
            metrics_section_label = NULL;
            saved_nets_label = NULL;
            appearance_section_label = NULL;
            theme_row_label = NULL;
            cursor_row_label = NULL;
            metrics_ip_ta = NULL;
            metrics_path_ta = NULL;
        }
    }

    void showSettingsScreen() {
        if (settings_content) {
            return;
        }

        const UiThemePalette &p = getThemePalette(app_settings.theme);

        settings_content = lv_obj_create(lv_scr_act());
        lv_obj_set_size(settings_content, screen_width, screen_height);
        lv_obj_align(settings_content, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(settings_content, 0, 0);
        lv_obj_set_style_bg_color(settings_content, lv_color_hex(p.settings_bg), 0);
        lv_obj_set_style_bg_opa(settings_content, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(settings_content, 10, 0);
        lv_obj_set_style_pad_row(settings_content, 8, 0);
        lv_obj_set_style_border_width(settings_content, 0, 0);
        lv_obj_set_scrollbar_mode(settings_content, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_flex_flow(settings_content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(settings_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        // 1. Header Row
        lv_obj_t *header_row = lv_obj_create(settings_content);
        lv_obj_set_size(header_row, lv_pct(100), 30);
        lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(header_row, 0, 0);
        lv_obj_set_style_pad_all(header_row, 0, 0);
        lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);

        settings_title_label = lv_label_create(header_row);
        lv_label_set_text(settings_title_label, "Settings");
        lv_obj_set_style_text_font(settings_title_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(settings_title_label, lv_color_hex(p.settings_text), 0);
        lv_obj_align(settings_title_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *close_btn = lv_btn_create(header_row);
        lv_obj_set_size(close_btn, 36, 28);
        lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(close_btn, settings_close_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "X");
        lv_obj_center(close_label);

        // 2. Metrics Config
        metrics_section_label = lv_label_create(settings_content);
        lv_label_set_text(metrics_section_label, "Metrics Config:");
        lv_obj_set_style_text_color(metrics_section_label, lv_color_hex(p.settings_text), 0);

        lv_obj_t *metrics_row = lv_obj_create(settings_content);
        lv_obj_set_size(metrics_row, lv_pct(100), 34);
        lv_obj_set_style_bg_opa(metrics_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(metrics_row, 0, 0);
        lv_obj_set_style_pad_all(metrics_row, 0, 0);
        lv_obj_set_flex_flow(metrics_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(metrics_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(metrics_row, LV_OBJ_FLAG_SCROLLABLE);

        metrics_ip_ta = lv_textarea_create(metrics_row);
        lv_textarea_set_one_line(metrics_ip_ta, true);
        lv_textarea_set_placeholder_text(metrics_ip_ta, "IP");
        lv_obj_set_size(metrics_ip_ta, 124, 32);
        lv_textarea_set_text(metrics_ip_ta, NetworkManager::getMetricsIP().c_str());
        tag_textarea_kind(metrics_ip_ta, InputFieldKind::NUMERIC_IP);
        lv_obj_add_event_cb(metrics_ip_ta, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(metrics_ip_ta, LV_SCROLLBAR_MODE_OFF);

        metrics_path_ta = lv_textarea_create(metrics_row);
        lv_textarea_set_one_line(metrics_path_ta, true);
        lv_textarea_set_placeholder_text(metrics_path_ta, "Path");
        lv_obj_set_size(metrics_path_ta, 100, 32);
        lv_textarea_set_text(metrics_path_ta, NetworkManager::getMetricsPath().c_str());
        tag_textarea_kind(metrics_path_ta, InputFieldKind::TEXT);
        lv_obj_add_event_cb(metrics_path_ta, textarea_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_set_scrollbar_mode(metrics_path_ta, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *m_save_btn = lv_btn_create(metrics_row);
        lv_obj_set_size(m_save_btn, 64, 32);
        lv_obj_add_event_cb(m_save_btn, [](lv_event_t * e) {
            (void)e;
            if (!metrics_ip_ta || !metrics_path_ta) return;
            String ip = lv_textarea_get_text(metrics_ip_ta);
            String path = lv_textarea_get_text(metrics_path_ta);
            if (ip.length() > 0 && path.length() > 0) {
                NetworkManager::setMetricsConfig(ip, path);
                hide_keyboard_and_restore();
                static const char * btns[] = {""};
                lv_obj_t * mbox = lv_msgbox_create(settings_content, "Saved", "Metrics config saved.", btns, true);
                lv_obj_center(mbox);
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *m_save_lbl = lv_label_create(m_save_btn);
        lv_label_set_text(m_save_lbl, "Save");
        lv_obj_center(m_save_lbl);

        // 3. Saved Networks Header + List
        lv_obj_t *nets_hdr_row = lv_obj_create(settings_content);
        lv_obj_set_size(nets_hdr_row, lv_pct(100), 30);
        lv_obj_set_style_bg_opa(nets_hdr_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(nets_hdr_row, 0, 0);
        lv_obj_set_style_pad_all(nets_hdr_row, 0, 0);
        lv_obj_clear_flag(nets_hdr_row, LV_OBJ_FLAG_SCROLLABLE);

        saved_nets_label = lv_label_create(nets_hdr_row);
        lv_label_set_text(saved_nets_label, "Saved Networks:");
        lv_obj_set_style_text_color(saved_nets_label, lv_color_hex(p.settings_text), 0);
        lv_obj_align(saved_nets_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *scan_btn = lv_btn_create(nets_hdr_row);
        lv_obj_set_size(scan_btn, 64, 28);
        lv_obj_align(scan_btn, LV_ALIGN_RIGHT_MID, -70, 0);
        lv_obj_add_event_cb(scan_btn, [](lv_event_t * e) {
            (void)e;
            show_wifi_scan_modal();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *scan_lbl = lv_label_create(scan_btn);
        lv_label_set_text(scan_lbl, "Scan");
        lv_obj_center(scan_lbl);

        lv_obj_t *add_btn = lv_btn_create(nets_hdr_row);
        lv_obj_set_size(add_btn, 64, 28);
        lv_obj_align(add_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(add_btn, [](lv_event_t * e) {
            (void)e;
            show_network_editor(-1, NULL);
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *add_lbl = lv_label_create(add_btn);
        lv_label_set_text(add_lbl, "Add");
        lv_obj_center(add_lbl);

        settings_list = lv_obj_create(settings_content);
        lv_obj_set_size(settings_list, lv_pct(100), 110);
        lv_obj_set_flex_flow(settings_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_gap(settings_list, 4, 0);
        lv_obj_set_style_pad_all(settings_list, 4, 0);
        lv_obj_set_style_bg_opa(settings_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(settings_list, 1, 0);
        lv_obj_set_style_border_color(settings_list, lv_color_hex(p.list_border), 0);
        lv_obj_set_scrollbar_mode(settings_list, LV_SCROLLBAR_MODE_AUTO);
        refresh_settings_list();

        // 4. Appearance & Touch Section
        appearance_section_label = lv_label_create(settings_content);
        lv_label_set_text(appearance_section_label, "Appearance & Touch:");
        lv_obj_set_style_text_color(appearance_section_label, lv_color_hex(p.settings_text), 0);

        lv_obj_t *theme_row = lv_obj_create(settings_content);
        lv_obj_set_size(theme_row, lv_pct(100), 36);
        lv_obj_set_style_bg_opa(theme_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(theme_row, 0, 0);
        lv_obj_set_style_pad_all(theme_row, 0, 0);
        lv_obj_clear_flag(theme_row, LV_OBJ_FLAG_SCROLLABLE);

        theme_row_label = lv_label_create(theme_row);
        lv_label_set_text(theme_row_label, "Theme:");
        lv_obj_set_style_text_color(theme_row_label, lv_color_hex(p.settings_text), 0);
        lv_obj_align(theme_row_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *theme_dd = lv_dropdown_create(theme_row);
        lv_dropdown_set_options(theme_dd, "Primitive\nDark High Contrast");
        lv_dropdown_set_selected(theme_dd, static_cast<uint16_t>(normalizeThemeId(app_settings.theme)));
        lv_obj_set_size(theme_dd, 180, 34);
        lv_obj_align(theme_dd, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_event_cb(theme_dd, [](lv_event_t * e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            lv_obj_t *dd = lv_event_get_target(e);
            int sel = static_cast<int>(lv_dropdown_get_selected(dd));
            apply_theme(sel);
            NetworkManager::saveAppSettings(app_settings);
        }, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *cursor_row = lv_obj_create(settings_content);
        lv_obj_set_size(cursor_row, lv_pct(100), 32);
        lv_obj_set_style_bg_opa(cursor_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cursor_row, 0, 0);
        lv_obj_set_style_pad_all(cursor_row, 0, 0);
        lv_obj_clear_flag(cursor_row, LV_OBJ_FLAG_SCROLLABLE);

        cursor_row_label = lv_label_create(cursor_row);
        lv_label_set_text(cursor_row_label, "Red Touch Dot:");
        lv_obj_set_style_text_color(cursor_row_label, lv_color_hex(p.settings_text), 0);
        lv_obj_align(cursor_row_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *cursor_sw = lv_switch_create(cursor_row);
        lv_obj_set_size(cursor_sw, 50, 26);
        lv_obj_align(cursor_sw, LV_ALIGN_RIGHT_MID, 0, 0);
        if (app_settings.touch_cursor) {
            lv_obj_add_state(cursor_sw, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(cursor_sw, [](lv_event_t * e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            lv_obj_t *sw = lv_event_get_target(e);
            app_settings.touch_cursor = lv_obj_has_state(sw, LV_STATE_CHECKED);
            if (!app_settings.touch_cursor && touch_cursor) {
                lv_obj_add_flag(touch_cursor, LV_OBJ_FLAG_HIDDEN);
            }
            NetworkManager::saveAppSettings(app_settings);
        }, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *calib_row = lv_obj_create(settings_content);
        lv_obj_set_size(calib_row, lv_pct(100), 32);
        lv_obj_set_style_bg_opa(calib_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(calib_row, 0, 0);
        lv_obj_set_style_pad_all(calib_row, 0, 0);
        lv_obj_set_flex_flow(calib_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(calib_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(calib_row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *reset_cal_btn = lv_btn_create(calib_row);
        lv_obj_set_size(reset_cal_btn, 140, 30);
        lv_obj_add_event_cb(reset_cal_btn, [](lv_event_t * e) {
            (void)e;
            if (touch_reset_cb) {
                touch_reset_cb();
            }
            static const char * btns[] = {""};
            lv_obj_t * mbox = lv_msgbox_create(settings_content, "Reset", "Touch calibration reset to default.", btns, true);
            lv_obj_center(mbox);
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *reset_cal_lbl = lv_label_create(reset_cal_btn);
        lv_label_set_text(reset_cal_lbl, "Reset TS Calib");
        lv_obj_center(reset_cal_lbl);

        lv_obj_t *calib_btn = lv_btn_create(calib_row);
        lv_obj_set_size(calib_btn, 146, 30);
        lv_obj_add_event_cb(calib_btn, [](lv_event_t * e) {
            (void)e;
            triggerTouchCalibration();
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *calib_lbl = lv_label_create(calib_btn);
        lv_label_set_text(calib_lbl, "Calibrate Touch");
        lv_obj_center(calib_lbl);
    }

    static void show_brightness_slider() {
        if (brightness_overlay) return;

        brightness_overlay = lv_obj_create(lv_scr_act());
        lv_obj_set_size(brightness_overlay, screen_width, screen_height);
        lv_obj_set_style_bg_color(brightness_overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(brightness_overlay, LV_OPA_40, 0);
        lv_obj_set_style_border_width(brightness_overlay, 0, 0);
        lv_obj_set_style_radius(brightness_overlay, 0, 0);
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

        Preferences prefs;
        if (prefs.begin("settings", true)) {
            initial_brightness_val = prefs.getInt("brightness", 128);
            prefs.end();
        } else {
            initial_brightness_val = 128;
        }
        lv_slider_set_value(slider, initial_brightness_val, LV_ANIM_OFF);

        lv_obj_add_event_cb(brightness_overlay, [](lv_event_t * e) {
            lv_obj_t * target = lv_event_get_target(e);
            lv_obj_t * current = lv_event_get_current_target(e);
            if (target == current) {
                tft.setBrightness(initial_brightness_val);
                lv_obj_del(brightness_overlay);
                brightness_overlay = NULL;
            }
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_add_event_cb(slider, [](lv_event_t * e) {
            lv_obj_t * slider_obj = lv_event_get_target(e);
            int v = lv_slider_get_value(slider_obj);
            tft.setBrightness(v);
        }, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t * close_btn = lv_btn_create(panel);
        lv_obj_set_size(close_btn, 60, 24);
        lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, 5);

        lv_obj_add_event_cb(close_btn, [](lv_event_t * e) {
            lv_obj_t * slider_obj = (lv_obj_t *)lv_event_get_user_data(e);
            int v = lv_slider_get_value(slider_obj);

            Preferences p;
            if (p.begin("settings", false)) {
                p.putInt("brightness", v);
                p.end();
            }

            lv_obj_del(brightness_overlay);
            brightness_overlay = NULL;
        }, LV_EVENT_CLICKED, slider);

        lv_obj_t * close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "OK");
        lv_obj_center(close_label);
    }

    void setTouchResetCallback(TouchResetCallback cb) {
        touch_reset_cb = cb;
    }

    bool isTouchCursorEnabled() {
        return app_settings.touch_cursor;
    }

    void init(uint16_t tft_width, uint16_t tft_height) {
        screen_width = tft_width;
        screen_height = tft_height;
        app_settings = NetworkManager::loadAppSettings();

        // Global persistent keyboard on top layer (160px height preserved for adult fingers)
        keyboard = lv_keyboard_create(lv_layer_top());
        lv_obj_set_size(keyboard, screen_width, screen_height / 2 + 40);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(keyboard, 191, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(keyboard, 191, LV_PART_ITEMS);
        lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_USER_1, ip_kb_map, ip_kb_ctrl);
        lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

        touch_cursor = lv_obj_create(lv_layer_sys());
        lv_obj_set_size(touch_cursor, 20, 20);
        lv_obj_set_style_radius(touch_cursor, 10, 0);
        lv_obj_set_style_bg_color(touch_cursor, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_bg_opa(touch_cursor, LV_OPA_80, 0);
        lv_obj_clear_flag(touch_cursor, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(touch_cursor, LV_OBJ_FLAG_HIDDEN);

        auto create_card = [](int idx, const char *title, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) {
            const int M = 8;
            int card_w = (screen_width - 3 * M) / 2;
            int card_h = static_cast<int>((screen_height - 3 * M) / 2 * 0.80f);

            lv_obj_t *c = lv_obj_create(lv_scr_act());
            lv_obj_set_size(c, card_w, card_h);
            lv_obj_set_align(c, align);
            lv_obj_set_style_pad_all(c, 8, 0);
            lv_obj_set_style_radius(c, 8, 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
            lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);

            lv_obj_t *t = lv_label_create(c);
            lv_label_set_text(t, title);
            lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
            lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 4);

            lv_obj_t *v = lv_label_create(c);
            lv_label_set_text(v, "--");
            lv_obj_set_style_text_font(v, &lv_font_montserrat_22, 0);
            lv_obj_align(v, LV_ALIGN_CENTER, 0, 2);

            lv_obj_t *sub = NULL;
            if (idx == 0 || idx == 3) {
                sub = lv_label_create(c);
                lv_label_set_text(sub, "");
                lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
                lv_obj_align(sub, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            }

            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, card_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_set_pos(c, x_ofs, y_ofs);

            card_obj[idx] = c;
            card_title[idx] = t;
            card_value[idx] = v;
            card_sub[idx] = sub;
        };

        create_card(0, "Voltage (V)", LV_ALIGN_TOP_LEFT, 7, 30);
        create_card(1, "Current (A)", LV_ALIGN_TOP_RIGHT, -7, 30);
        create_card(2, "Energy (kWh)", LV_ALIGN_BOTTOM_LEFT, 7, -30);
        create_card(3, "Power (W)", LV_ALIGN_BOTTOM_RIGHT, -7, -30);

        // Floating Top-50px HUD (renders over top edges without reserving card space)
        lv_obj_t *hud_layer = lv_scr_act();

        // Top-Left: Freshness Dot + Wi-Fi Bars Pill (tappable for diagnostics)
        hud_status_btn = lv_btn_create(hud_layer);
        lv_obj_set_size(hud_status_btn, 90, 36);
        lv_obj_align(hud_status_btn, LV_ALIGN_TOP_LEFT, 4, 6);
        lv_obj_set_style_radius(hud_status_btn, 18, 0);
        lv_obj_set_style_bg_opa(hud_status_btn, LV_OPA_80, 0);
        lv_obj_set_style_pad_all(hud_status_btn, 0, 0);
        lv_obj_set_style_pad_left(hud_status_btn, 8, 0);
        lv_obj_set_style_pad_right(hud_status_btn, 8, 0);
        lv_obj_set_flex_flow(hud_status_btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(hud_status_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(hud_status_btn, 5, 0);
        lv_obj_add_event_cb(hud_status_btn, [](lv_event_t * e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                show_diagnostics_modal();
            }
        }, LV_EVENT_CLICKED, NULL);

        hud_freshness_dot = lv_obj_create(hud_status_btn);
        lv_obj_set_size(hud_freshness_dot, 10, 10);
        lv_obj_set_style_radius(hud_freshness_dot, 5, 0);
        lv_obj_set_style_border_width(hud_freshness_dot, 0, 0);
        lv_obj_set_style_bg_color(hud_freshness_dot, lv_color_hex(0xF39C12), 0);
        lv_obj_set_style_bg_opa(hud_freshness_dot, LV_OPA_COVER, 0);
        lv_obj_clear_flag(hud_freshness_dot, LV_OBJ_FLAG_CLICKABLE);

        hud_wifi_label = lv_label_create(hud_status_btn);
        lv_label_set_text(hud_wifi_label, LV_SYMBOL_WIFI " Off");
        lv_obj_set_style_text_font(hud_wifi_label, &lv_font_montserrat_14, 0);

        // Top-Center: Ambient DHT22 Temperature & Humidity Pill
        hud_ambient_pill = lv_obj_create(hud_layer);
        lv_obj_set_size(hud_ambient_pill, LV_SIZE_CONTENT, 32);
        lv_obj_align(hud_ambient_pill, LV_ALIGN_TOP_MID, 3, 8);
        lv_obj_set_style_radius(hud_ambient_pill, 16, 0);
        lv_obj_set_style_border_width(hud_ambient_pill, 0, 0);
        lv_obj_set_style_bg_opa(hud_ambient_pill, LV_OPA_80, 0);
        lv_obj_set_style_pad_hor(hud_ambient_pill, 8, 0);
        lv_obj_set_style_pad_ver(hud_ambient_pill, 4, 0);
        lv_obj_set_scrollbar_mode(hud_ambient_pill, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(hud_ambient_pill, LV_OBJ_FLAG_CLICKABLE);

        hud_ambient_label = lv_label_create(hud_ambient_pill);
        lv_label_set_text(hud_ambient_label, "--\xC2\xB0C \xE2\x80\xA2 --%");
        lv_obj_set_style_text_font(hud_ambient_label, &lv_font_montserrat_14, 0);
        lv_obj_center(hud_ambient_label);

        // Top-Right: Brightness & Settings Buttons
        lv_obj_t *brightness_button = lv_btn_create(hud_layer);
        lv_obj_set_size(brightness_button, 40, 40);
        lv_obj_set_style_radius(brightness_button, 20, 0);
        lv_obj_align(brightness_button, LV_ALIGN_TOP_RIGHT, -48, 4);
        lv_obj_add_event_cb(brightness_button, [](lv_event_t * e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                show_brightness_slider();
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *brightness_label = lv_label_create(brightness_button);
        lv_label_set_text(brightness_label, LV_SYMBOL_EYE_OPEN);
        lv_obj_center(brightness_label);

        lv_obj_t *settings_button = lv_btn_create(hud_layer);
        lv_obj_set_size(settings_button, 40, 40);
        lv_obj_set_style_radius(settings_button, 20, 0);
        lv_obj_align(settings_button, LV_ALIGN_TOP_RIGHT, -4, 4);
        lv_obj_add_event_cb(settings_button, [](lv_event_t * e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                showSettingsScreen();
            }
        }, LV_EVENT_CLICKED, NULL);
        lv_obj_t *settings_label = lv_label_create(settings_button);
        lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
        lv_obj_center(settings_label);

        apply_theme(app_settings.theme);
    }

    void update(const MetricsData &data) {
        if (std::isfinite(data.voltage)) {
            String s = String(data.voltage, 2) + " V";
            lv_label_set_text(card_value[0], s.c_str());
        } else {
            lv_label_set_text(card_value[0], "--");
        }

        if (card_sub[0]) {
            if (std::isfinite(data.frequency)) {
                String s = String(data.frequency, 1) + " Hz";
                lv_label_set_text(card_sub[0], s.c_str());
            } else {
                lv_label_set_text(card_sub[0], "");
            }
        }

        if (std::isfinite(data.current)) {
            String s = String(data.current, 2) + " A";
            lv_label_set_text(card_value[1], s.c_str());
        } else {
            lv_label_set_text(card_value[1], "--");
        }

        if (std::isfinite(data.energy)) {
            String s = String(data.energy, 2) + " kWh";
            lv_label_set_text(card_value[2], s.c_str());
        } else {
            lv_label_set_text(card_value[2], "--");
        }

        if (std::isfinite(data.wattage)) {
            String s = String(data.wattage, 2) + " W";
            lv_label_set_text(card_value[3], s.c_str());
        } else {
            lv_label_set_text(card_value[3], "--");
        }

        if (card_sub[3]) {
            if (std::isfinite(data.power_factor)) {
                String s = "PF " + String(data.power_factor, 2);
                lv_label_set_text(card_sub[3], s.c_str());
            } else {
                lv_label_set_text(card_sub[3], "");
            }
        }

        if (hud_ambient_label) {
            String t_str = std::isfinite(data.temperature) ? String(data.temperature, 1) : String("--");
            String h_str = std::isfinite(data.humidity) ? String(data.humidity, 0) : String("--");
            lv_label_set_text_fmt(
                hud_ambient_label,
                "%s\xC2\xB0C \xE2\x80\xA2 %s%%",
                t_str.c_str(),
                h_str.c_str());
        }

        if (zoom_detail && zoom_card_index >= 0) {
            lv_obj_t *detail_val = (lv_obj_t*)lv_obj_get_user_data(zoom_detail);
            if (detail_val) {
                lv_label_set_text(detail_val, lv_label_get_text(card_value[zoom_card_index]));
            }
        }
    }

    void updateDiagnostics(const NetworkManager::NetworkDiagnostics &diag) {
        if (!hud_freshness_dot || !hud_wifi_label) {
            return;
        }

        uint32_t now_ms = millis();
        HudFreshness fresh = computeHudFreshness(
            diag.wifi_connected, diag.has_valid_metrics, diag.fetch_failed, diag.last_success_ms, now_ms);
        lv_obj_set_style_bg_color(hud_freshness_dot, lv_color_hex(getFreshnessDotColor(fresh)), 0);

        int bars = rssiToSignalBars(diag.wifi_connected, diag.rssi_dbm);
        if (bars <= 0) {
            lv_label_set_text(hud_wifi_label, LV_SYMBOL_WIFI " Off");
        } else {
            lv_label_set_text_fmt(hud_wifi_label, LV_SYMBOL_WIFI " %d/4", bars);
        }
    }

    void setFetchError(bool error) {
        (void)error;
    }
}
