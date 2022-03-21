#pragma once

#include <Arduino.h>
#if defined(ESP32)
#pragma message "Building KnxLed for ESP32"
#include "driver/ledc.h"
extern byte nextEsp32LedChannel; // next available LED channel for ESP32
#elif defined(ESP8266)
#pragma message "Building KnxLed for ESP8266"
#else
#error "Wrong hardware. Not ESP8266 or ESP32"
#endif

#define DIMM_STOP 0
#define DIMM_DOWN 1
#define DIMM_STOP2 8
#define DIMM_UP 9
#define DIMM_UNSET -1

#define MIN_BRIGHTNESS 12
#define MAX_BRIGHTNESS 255

#define min_f(a, b, c)  (fminf(a, fminf(b, c)))
#define max_f(a, b, c)  (fmaxf(a, fmaxf(b, c)))

enum __cctMode { normal, bipolar, tempChannel }; //CCT mode: normal (2 separate channels), bipolar (2 instead of 3 wires), temperature control channel (CH1=brightness, CH2=temperature)

typedef struct __hsv {
    uint16_t h; //hue
    uint8_t s; //sat
    uint8_t v; //val==brightness

    void fromDPT232600 (uint32_t raw) {
        h = round(((raw >> 16) & 0xFF) * 359.0f/255.0f); // convert knx hsv from 0..255 -> 0..359
        s = (raw >> 8) & 0xFF;
        v = raw & 0xFF;
    }

    uint32_t toDPT232600 (void) {
        uint8_t _h = round(h*255.0f/359.0); // convert to knx hsv from 0..359 -> 0..255
        return (_h << 16) | (s << 8) | v;
    }

    inline bool operator!=(const __hsv &cmp) const {
        return h != cmp.h || s != cmp.s || v != cmp.v;
    }

    int changedPercent (const __hsv &cmp) {
        uint16_t _h = round(abs(h - cmp.h) * 359.0f / 100.0f);
        uint8_t _s = round(abs(s - cmp.s) * 255.0f / 100.0f);
        uint8_t _v = round(abs(v - cmp.v) * 255.0f / 100.0f);
        return max(_h, (uint16_t)max(_s,_v));
    }
} hsv_t;

typedef struct __rgb
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;

    void fromDPT232600 (uint32_t raw) {
        red = (raw >> 16) & 0xFF;
        green = (raw >> 8) & 0xFF;
        blue = raw & 0xFF;
    }

    uint32_t toDPT232600 (void) {
        return (red << 16) | (green << 8) | blue;
    }
    inline bool operator!=(const __rgb &cmp) const {
        return (red != cmp.red || green != cmp.green || blue != cmp.blue);
    }
    int changedPercent (const __rgb &cmp) {
        uint8_t _r = round(abs(blue - cmp.blue) * 255.0f / 100.0f);
        uint8_t _g = round(abs(green - cmp.green) * 255.0f / 100.0f);
        uint8_t _b = round(abs(blue - cmp.blue) * 255.0f / 100.0f);
        return max(_r, max(_g,_b));
    }
} rgb_t;

typedef struct __kelvinTableRGB {
	uint kelvin;
	uint8_t red;
	uint8_t green;
	uint8_t blue;

    inline bool operator==(const uint cmp) const {
        uint c = cmp - (cmp % 100); // round to nearest value
        return (kelvin == c);
    }
} kelvinTable_t;

typedef void callbackBool(bool);
typedef void callbackInt(int);
typedef void callbackRgb(rgb_t);
typedef void callbackHsv(hsv_t);

class KnxLed
{
public:
    enum LightTypes
    {
        SWITCHABLE,
        DIMMABLE,
        TUNABLEWHITE,
        RGB,
        RGBW,
        RGBCT
    };
    
    enum LightMode {        
        MODE_CCT,
        MODE_RGB
    };

    void initSwitchableLight(uint8_t switchPin);
    void initDimmableLight(uint8_t ledPin);
    void initTunableWhiteLight(uint8_t cwPin, uint8_t wwPin, __cctMode cctMode);
    void initRgbLight(uint8_t rPin, uint8_t gPin, uint8_t bPin);
    void initRgbwLight(uint8_t rPin, uint8_t gPin, uint8_t bPin, uint8_t wPin, bool isColdWhite);
    void initRgbcctLight(uint8_t rPin, uint8_t gPin, uint8_t bPin, uint8_t cwPin, uint8_t wwPin, __cctMode cctMode);

    void configDefaultBrightness(int brightness);
    void configDefaultTemperature(int temperature);
    void configDefaultHsv (hsv_t hsv);
    void configDimmSpeed(unsigned int dimmSetSpeed);

    void registerStatusCallback(callbackBool *fctn);
    void registerBrightnessCallback(callbackInt *fctn);
    void registerTemperatureCallback(callbackInt *fctn);
    void registerColorRgbCallback(callbackRgb *fctn);
    void registerColorHsvCallback(callbackHsv *fctn);

    void switchLight(bool state);
    void setBrightness(int brightness);
    void setBrightness(int brightness, bool saveValue);
    void setTemperature(int temperature);
    void setRgb(rgb_t rgb);
    void setHsv (hsv_t hsv);
    
    void setRelDimmCmd(int dimmCmd);
    void setRelTemperatureCmd(int temperatureCmd);

    bool getSwitchState();
    int getBrightness();
    int getTemperature();
    rgb_t getRgb();
    hsv_t getHsv();
    
    void loop();

private:
    bool initialized = false;
    LightTypes lightType;
    byte outputPins[5];
#if defined(ESP32)
    unsigned int pwmFrequency = 5000; // 5kHz
    unsigned int pwmResolution = 10;  // 2^10 = 1024
    ledc_channel_t esp32LedCh[5];
#elif defined(ESP8266)
    unsigned int pwmFrequency = 2000;  // 2kHz bei Library >=3.0.0, 50Hz bei Library 2.6.3
    unsigned int pwmResolution = 1023; // Default is 1023
                                       // All 1022 PWM steps are available at 977Hz, 488Hz, 325Hz, 244Hz, 195Hz, 162Hz, 139Hz, 122Hz, 108Hz, 97Hz, 88Hz, 81Hz, 75Hz, etc.
    // Calculation = truncate(1/(1E-6 * 1023)) for the PWM frequencies with all (or most) discrete PWM steps. (master)
#endif
    unsigned int dimmSpeed = 6;
    int defaultBrightness = MAX_BRIGHTNESS;
    int savedBrightness = 0;
    int setpointBrightness = -1;
    int actBrightness = -1;
    //rgb_t defaultRgb;
    //rgb_t savedRgb;
    //rgb_t setpointRgb;
    //rgb_t actRgb;

    hsv_t defaultHsv;
    hsv_t savedHsv;
    hsv_t setpointHsv;
    hsv_t actHsv;

    int defaultTemperature = 3500;
    int setpointTemperature = defaultTemperature;
    int actTemperature = defaultTemperature;

    bool isTwBipolar = false; // Tunable White with 2-Wires and different polarity for each channel
    bool isTwTempCh = false;  // Tunable White with brightness channel and temperature channel
    bool isRgbwColdWhite = false;   //White LED on RGBW is warm or cold white
    LightMode currentLightMode = MODE_CCT;
    
    unsigned int dimmCount = 0;
    int relDimmCmd = DIMM_UNSET;
    int relTemperatureCmd = DIMM_UNSET;

    callbackBool *returnStatusFctn;
    callbackInt *returnBrightnessFctn;
    callbackInt *returnTemperatureFctn;
    callbackRgb *returnColorRgbFctn;
    callbackHsv *returnColorHsvFctn;
    
    void initOutputChannels (uint8_t usedChannels);
    void fade();
    void pwmControl();
    void ledAnalogWrite(byte channel, int duty);
    void rgb2hsv(const rgb_t rgb, hsv_t &hsv);
    void hsv2rgb(const hsv_t hsv, rgb_t &rgb);
};