#include "Display_LGFX.h"

LGFX::LGFX(void)
{
    {
        auto cfg = _bus_instance.config();
        cfg.freq_write = 20000000; // Lowered from 25MHz to improve parallel bus stability and reduce screen flickering
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
        cfg.readable = false;
        cfg.invert = false;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = true;

        _panel_instance.config(cfg);
    }

    {
        auto cfg = _light_instance.config();
        cfg.pin_bl = 0;
        cfg.invert = false;
        cfg.freq = 44100;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
}

LGFX tft;
