Based on the provided schematics for the ESP32-2432S022 module, here is the technical breakdown of how the TFT display backlight is wired and how you can dim it.

### **Hardware Analysis**

According to the **ESP32-2432022-LCM-V1.0** schematic, the backlight is controlled by **GPIO0** (labeled as `IO0`).

Here is how the circuit is laid out:

* **The LED Backlight:** The display's backlight anodes (`LED+` on pin 17 of the LCD connector) are tied directly to `3.3V`.
* **The Control Circuit:** The cathodes (`LED1-` through `LED4-` on pins 18-21) are routed to the drain of an N-channel MOSFET (`Q2`, an AO3402).
* **The Trigger:** The gate of this MOSFET is connected directly to the ESP32's **GPIO0**.

Because the MOSFET acts as a switch on the low side (ground side) of the LEDs, driving GPIO0 **HIGH** turns the backlight ON, and driving it **LOW** turns the backlight OFF.

### **How to Dim the Backlight**

To achieve dimming rather than just on/off control, you need to output a **PWM (Pulse Width Modulation)** signal on GPIO0. By rapidly switching the pin high and low, the ratio of "on time" to "off time" (duty cycle) dictates the perceived brightness of the screen.

#### **Implementation in Arduino IDE**

Depending on which version of the ESP32 board manager core you have installed, the code for PWM differs slightly.

**For ESP32 Core v3.x.x (Newest):**
The standard `analogWrite()` function is now natively supported.

```cpp
const int backlightPin = 0; // GPIO0

void setup() {
  pinMode(backlightPin, OUTPUT);
  // Set brightness to ~50% (127 out of 255)
  analogWrite(backlightPin, 127); 
}

void loop() {
  // You can dynamically adjust brightness here
}

```

**For ESP32 Core v2.x.x (Older):**
You must use the ESP32's specific LEDC API.

```cpp
const int backlightPin = 0; // GPIO0
const int pwmChannel = 0;
const int pwmFreq = 5000;   // 5 kHz frequency is standard for LED dimming
const int pwmResolution = 8; // 8-bit resolution (0-255)

void setup() {
  // Configure the PWM timer
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  
  // Attach the channel to the GPIO
  ledcAttachPin(backlightPin, pwmChannel);
  
  // Set brightness to ~50% (127 out of 255)
  ledcWrite(pwmChannel, 127);
}

void loop() {
  // Change ledcWrite(pwmChannel, value) to adjust dynamically
}

```

### **Important Hardware Quirk**

You should be aware that **GPIO0** is also the ESP32's primary strapping pin for boot modes, and it is physically connected to the **BOOT button** on your board (as seen in the same schematic).

* If you press the BOOT button while the device is running, it connects GPIO0 to GND. This means pressing the BOOT button will temporarily turn off your backlight while it is held down.
* Ensure that nothing external pulls GPIO0 low during a reset, or the board will enter flashing mode instead of running your code.