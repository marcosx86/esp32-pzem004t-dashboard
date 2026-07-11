#ifndef DISPLAY_LGFX_H
#define DISPLAY_LGFX_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
private:
    lgfx::Panel_ST7789 _panel_instance; // ST7789UI
    lgfx::Bus_Parallel8 _bus_instance;  // MCU8080 8B
    lgfx::Light_PWM _light_instance;

public:
    LGFX(void);
};

extern LGFX tft;

#endif // DISPLAY_LGFX_H
