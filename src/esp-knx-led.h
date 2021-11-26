#pragma once

#include <Arduino.h>
#if defined(ESP32)
    #pragma message "Building KnxLed for ESP32"
	#include "driver/ledc.h"
	extern byte nextEsp32LedChannel; //next available LED channel for ESP32
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

const int MIN_BRIGHTNESS = 12;
const int MAX_BRIGHTNESS = 255;

typedef void callbackBool(bool);
typedef void callbackInt(int);

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
        RGBWW
    };

    void initSwitchableLight(byte switchPin);
    void initDimmableLight(byte ledPin);
    void initTunableWhiteLight(byte cwPin, byte wwPin);
    void initRgbLight(byte rPin, byte gPin, byte bPin);
    void initRgbwLight(byte rPin, byte gPin, byte bPin, byte wPin);
    void initRgbwwLight(byte rPin, byte gPin, byte bPin, byte cwPin, byte wwPin);
    void setupTwBipolar();
    void setupTwTempCh();
    
    void configDefaultBrightness(int brightness);
    void configDefaultTemperature(int temperature);
    void configDimmSpeed(unsigned int dimmSetSpeed);

    void registerStatusCallback(callbackBool* fctn);
    void registerBrightnessCallback(callbackInt* fctn);
    void registerTemperatureCallback(callbackInt* fctn);

    void switchLight(bool state);
    void setBrightness(int brightness);
    void setBrightness(int brightness, bool saveValue);
    void setTemperature(int temperature);
    void setRelDimmCmd(int dimmCmd);
    void setRelTemperatureCmd(int temperatureCmd);    

    bool getSwitchState();
    int getBrightness();
    int getTemperature();

    void loop();

private:
    bool initialized = false;
    LightTypes lightType;
    byte outputPins[5];
    #if defined(ESP32)
        unsigned int pwmFrequency = 5000;   // 5kHz
        unsigned int pwmResolution = 10;    // 2^10 = 1024
        ledc_channel_t esp32LedCh[5];
    #elif defined(ESP8266)
        unsigned int pwmFrequency = 2000;   // 2kHz bei Library >=3.0.0, 50Hz bei Library 2.6.3
        unsigned int pwmResolution = 1023;  // Default is 1023
        //All 1022 PWM steps are available at 977Hz, 488Hz, 325Hz, 244Hz, 195Hz, 162Hz, 139Hz, 122Hz, 108Hz, 97Hz, 88Hz, 81Hz, 75Hz, etc.
        //Calculation = truncate(1/(1E-6 * 1023)) for the PWM frequencies with all (or most) discrete PWM steps. (master)
	#endif    
    unsigned int dimmSpeed = 6;
    int defaultBrightness = MAX_BRIGHTNESS;   
    int savedBrightness = 0;
    int setpointBrightness = -1;
    int actBrightness = -1;

    int defaultTemperature = 3500;
    int setpointTemperature = defaultTemperature;
    int actTemperature = defaultTemperature;
    
    bool isTwBipolar = false; //Tunable White with only 2 wires
    bool isTwTempCh = false; //Tunable White with brightness channel and temperature channel

    unsigned int dimmCount = 0;
    int relDimmCmd = DIMM_UNSET;
    int relTemperatureCmd = DIMM_UNSET;

    callbackBool* returnStatusFctn;
    callbackInt* returnBrightnessFctn;
    callbackInt* returnTemperatureFctn;
    void fade();
    void pwmControl();
    void ledAnalogWrite(byte channel, int duty);
};