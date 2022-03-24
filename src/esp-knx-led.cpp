#include "esp-knx-led.h"
#if defined(ESP32)
byte nextEsp32LedChannel = LEDC_CHANNEL_0; // next available LED channel for ESP32
#endif

// lookup table for kelvin to rgb conversion
// crashes when using PROGMEM here
const kelvinTable_t kelvinTable[] = {
	{0, 0, 0, 0},
	{2700, 255, 169, 87},
	{2800, 255, 173, 94},
	{2900, 255, 177, 101},
	{3000, 255, 180, 107},
	{3100, 255, 184, 114},
	{3200, 255, 187, 120},
	{3300, 255, 190, 126},
	{3400, 255, 193, 132},
	{3500, 255, 196, 137},
	{3600, 255, 199, 143},
	{3700, 255, 201, 148},
	{3800, 255, 204, 153},
	{3900, 255, 206, 159},
	{4000, 255, 209, 163},
	{4100, 255, 211, 168},
	{4200, 255, 213, 173},
	{4300, 255, 215, 177},
	{4400, 255, 217, 182},
	{4500, 255, 219, 186},
	{4600, 255, 221, 190},
	{4700, 255, 223, 194},
	{4800, 255, 225, 198},
	{4900, 255, 227, 202},
	{5000, 255, 228, 206},
	{5100, 255, 230, 210},
	{5200, 255, 232, 213},
	{5300, 255, 233, 217},
	{5400, 255, 235, 220},
	{5500, 255, 236, 224},
	{5600, 255, 238, 227},
	{5700, 255, 239, 230},
	{5800, 255, 240, 233},
	{5900, 255, 242, 236},
	{6000, 255, 243, 239},
	{6100, 255, 244, 242},
	{6200, 255, 245, 245},
	{6300, 255, 246, 247},
	{6400, 255, 248, 251},
	{6500, 255, 249, 253},
};

// lookup table for logarithmic dimming curve
const PROGMEM int lookupTable[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 36, 37, 38, 39, 41, 42, 43, 44, 45,
	47, 48, 49, 51, 52, 53, 54, 56, 57, 59, 60, 61, 63, 64, 66, 67, 69, 70, 72, 73,
	75, 76, 78, 79, 81, 83, 84, 86, 88, 89, 91, 93, 95, 96, 98, 100, 102, 104, 106,
	108, 109, 111, 113, 115, 117, 120, 122, 124, 126, 128, 130, 132, 135, 137, 139,
	142, 144, 146, 149, 151, 154, 156, 159, 161, 164, 166, 169, 172, 174, 177, 180,
	183, 185, 188, 191, 194, 197, 200, 203, 206, 209, 212, 215, 219, 222, 225, 228,
	232, 235, 238, 242, 245, 249, 252, 256, 260, 263, 267, 271, 275, 278, 282, 286,
	290, 294, 298, 302, 306, 310, 315, 319, 323, 327, 332, 336, 341, 345, 350, 354,
	359, 364, 368, 373, 378, 383, 388, 392, 397, 403, 408, 413, 418, 423, 428, 434,
	439, 445, 450, 456, 461, 467, 472, 478, 484, 490, 496, 502, 508, 514, 520, 526,
	532, 538, 545, 551, 557, 564, 570, 577, 584, 590, 597, 604, 611, 618, 625, 632,
	639, 646, 653, 660, 668, 675, 683, 690, 698, 705, 713, 721, 729, 736, 744, 752,
	760, 769, 777, 785, 793, 802, 810, 819, 827, 836, 845, 853, 862, 871, 880, 889,
	898, 907, 917, 926, 935, 945, 954, 964, 973, 983, 993, 1003, 1013, 1023};

void KnxLed::switchLight(bool state)
{
	switch (lightType)
	{
	case SWITCHABLE:
	{
		setBrightness(int(state));
		break;
	}
	case DIMMABLE:
	case TUNABLEWHITE:
	{
		if (state)
		{
			if (defaultBrightness >= MIN_BRIGHTNESS)
			{
				setBrightness(defaultBrightness);
			}
			// If default brightness is set to 0, the last brightness will be restored
			else if (savedBrightness >= MIN_BRIGHTNESS)
			{
				setBrightness(savedBrightness);
			}
			else
			{
				setBrightness(MAX_BRIGHTNESS);
			}

			if (lightType == TUNABLEWHITE && defaultTemperature > 0)
			{
				setTemperature(defaultTemperature);
			}
		}
		else
		{
			if (setpointBrightness > 0)
			{
				savedBrightness = setpointBrightness;
			}
			setBrightness(0);
		}
		break;
	}
	case RGB:
	case RGBW:
	case RGBCT:
	{
		if (state)
		{
			hsv_t hsv;
			if (defaultHsv.v >= MIN_BRIGHTNESS)
			{
				hsv = defaultHsv;
			}
			// If default brightness is set to 0, the last brightness will be restored
			else if (savedHsv.v >= MIN_BRIGHTNESS)
			{
				hsv = savedHsv;
			}
			else
			{
				hsv = {50, 255, 255};
			}
			setHsv(hsv);
		}
		else
		{
			if (setpointBrightness > 0)
			{
				savedHsv = setpointHsv;
			}
			setBrightness(0);
		}
		break;
	}
	}
}

void KnxLed::setBrightness(int brightness)
{
	setBrightness(brightness, true);
}

void KnxLed::setBrightness(int brightness, bool saveValue)
{
	if (brightness != setpointBrightness)
	{
		if (brightness >= 0 && brightness <= MAX_BRIGHTNESS)
		{
			setpointBrightness = brightness;
			if (setpointBrightness > 0 && saveValue)
			{
				savedBrightness = setpointBrightness;
			}
		}
		if (returnBrightnessFctn != nullptr)
		{
			returnBrightnessFctn(setpointBrightness);
		}
		relDimmCmd = DIMM_UNSET;
		relTemperatureCmd = DIMM_UNSET;
		setpointHsv.v = brightness;
	}
}

void KnxLed::setTemperature(int temperature)
{
	if (lightType != TUNABLEWHITE && lightType != RGBW && lightType != RGBCT)
		return;

	if (temperature >= 2700 && temperature <= 6500)
	{
		setpointTemperature = temperature;
	}
	if (returnTemperatureFctn != nullptr)
	{
		returnTemperatureFctn(setpointTemperature);
	}
	relDimmCmd = DIMM_UNSET;
	relTemperatureCmd = DIMM_UNSET;
	if (currentLightMode != MODE_CCT)
	{
		actTemperature = setpointTemperature;
		currentLightMode = MODE_CCT;
	}
}

// set RGB value. This will be converted to HSV internally
void KnxLed::setRgb(rgb_t rgb)
{
	hsv_t _hsv;
	if (rgb.red + rgb.green + rgb.blue == 0)
	{
		_hsv.h = actHsv.h;
		_hsv.s = actHsv.s;
		_hsv.v = 0;
	}
	else
	{
		rgb2hsv(rgb, _hsv);
	}
	setHsv(_hsv);
}

// set HSV value.
void KnxLed::setHsv(hsv_t hsv)
{
	setpointHsv = hsv;
	if (actHsv.v == 0)
	{
		actHsv.h = hsv.h;
		actHsv.s = hsv.s;
	}

	if (returnColorHsvFctn != nullptr)
	{
		returnColorHsvFctn(setpointHsv);
	}
	if (returnColorRgbFctn != nullptr)
	{
		rgb_t _rgb;
		hsv2rgb(hsv, _rgb);
		returnColorRgbFctn(_rgb);
	}
	relDimmCmd = DIMM_UNSET;
	relTemperatureCmd = DIMM_UNSET;
	currentLightMode = MODE_RGB;
	setBrightness(hsv.v);
}

void KnxLed::configDefaultBrightness(int brightness)
{
	if (brightness >= 0 && brightness <= MAX_BRIGHTNESS)
	{
		defaultBrightness = brightness;
	}
}

void KnxLed::configDefaultTemperature(int temperature)
{
	if (temperature == 0 || (temperature >= 2700 && temperature <= 6500))
	{
		defaultTemperature = temperature;
	}
}

void KnxLed::configDefaultHsv(hsv_t hsv)
{
	defaultHsv = hsv;
}

void KnxLed::configDimmSpeed(unsigned int dimmSetSpeed)
{
	dimmSpeed = dimmSetSpeed;
}

void KnxLed::setRelDimmCmd(int dimmCmd)
{
	if (dimmCmd >= DIMM_UP)
		relDimmCmd = DIMM_UP;
	else if (dimmCmd >= DIMM_DOWN && dimmCmd < DIMM_STOP2)
		relDimmCmd = DIMM_DOWN;
	else
		relDimmCmd = DIMM_STOP;
}

void KnxLed::setRelTemperatureCmd(int temperatureCmd)
{
	if (temperatureCmd >= DIMM_UP)
		relTemperatureCmd = DIMM_UP;
	else if (temperatureCmd >= DIMM_DOWN && temperatureCmd < DIMM_STOP2)
		relTemperatureCmd = DIMM_DOWN;
	else
		relTemperatureCmd = DIMM_STOP;
}

void KnxLed::loop()
{
	if (initialized)
	{
		fade();
	}
}

void KnxLed::fade()
{
	dimmCount++;
	int oldBrightness = actBrightness;
	if (dimmCount >= dimmSpeed)
	{
		dimmCount = 0;
		if (relDimmCmd == DIMM_UP && actBrightness < MAX_BRIGHTNESS)
		{
			setpointBrightness = actBrightness + 1;
			if (returnBrightnessFctn != nullptr && (int)(setpointBrightness / 2.55 * 2 + 0.7) % 20 == 0)
			{
				returnBrightnessFctn(setpointBrightness);
			}
		}
		else if (relDimmCmd == DIMM_DOWN && actBrightness > MIN_BRIGHTNESS)
		{
			setpointBrightness = actBrightness - 1;
			if (returnBrightnessFctn != nullptr && (int)(setpointBrightness / 2.55 * 2 + 0.7) % 20 == 0)
			{
				returnBrightnessFctn(setpointBrightness);
			}
		}
		else if (relDimmCmd == DIMM_STOP || relDimmCmd == DIMM_STOP2)
		{
			savedBrightness = setpointBrightness; // = actBrightness;
			if (returnBrightnessFctn != nullptr)
			{
				returnBrightnessFctn(setpointBrightness);
			}
			relDimmCmd = DIMM_UNSET;
		}

		if (relTemperatureCmd == DIMM_UP && actTemperature < 6500)
		{
			setpointTemperature = min(actTemperature + 20, 6500);
			if (returnTemperatureFctn != nullptr && setpointTemperature % 300 == 0)
			{
				returnTemperatureFctn(setpointTemperature);
			}
		}
		else if (relTemperatureCmd == DIMM_DOWN && actTemperature > 2700)
		{
			setpointTemperature = max(actTemperature - 20, 2700);
			if (returnTemperatureFctn != nullptr && setpointTemperature % 300 == 0)
			{
				returnTemperatureFctn(setpointTemperature);
			}
		}
		else if (relTemperatureCmd == DIMM_STOP || relTemperatureCmd == DIMM_STOP2)
		{
			if (returnTemperatureFctn != nullptr)
			{
				returnTemperatureFctn(setpointTemperature);
			}
			relTemperatureCmd = DIMM_UNSET;
		}
	}

	bool updatePwm = false;
	if (setpointBrightness != actBrightness)
	{
		actBrightness += setpointBrightness > actBrightness ? 1 : -1;
		updatePwm = true;
	}

	if (setpointTemperature > actTemperature)
	{
		actTemperature = min(actTemperature + 20, setpointTemperature);
		updatePwm = true;
	}
	else if (setpointTemperature < actTemperature)
	{
		actTemperature = max(actTemperature - 20, setpointTemperature);
		updatePwm = true;
	}

	if (setpointHsv.h != actHsv.h)
	{
		bool do360overflow = abs(setpointHsv.h - actHsv.h) > 180;
		if ((setpointHsv.h > actHsv.h) != do360overflow)
		{
			actHsv.h = (actHsv.h + 1) % 360;
		}
		else
		{
			actHsv.h = (actHsv.h + 359) % 360;
		}
		updatePwm = true;
	}

	if (setpointHsv.s != actHsv.s)
	{
		actHsv.s += setpointHsv.s > actHsv.s ? 1 : -1;
		updatePwm = true;
	}

	if (currentLightMode == MODE_RGB)
	{
		if (setpointBrightness != actHsv.v)
		{
			actHsv.v += setpointBrightness > actHsv.v ? 1 : -1;
			updatePwm = true;
		}
	}
	else
	{
		if (actHsv.v > 0)
		{
			actHsv.v -= 1;
			updatePwm = true;
		}
	}

	// to avoid flickering, only update on change
	if (updatePwm)
	{
		pwmControl();
		if (returnStatusFctn != nullptr)
		{
			if (actBrightness == 0 && oldBrightness != 0)
			{
				returnStatusFctn(false);
			}
			else if (oldBrightness == 0 && actBrightness != 0)
			{
				returnStatusFctn(true);
			}
		}
	}
}

void KnxLed::pwmControl()
{
	switch (lightType)
	{
	case SWITCHABLE:
	{
		digitalWrite(outputPins[0], actBrightness > 0);
		break;
	}
	case DIMMABLE:
	{
		int dutyCh0 = actBrightness;
		ledAnalogWrite(0, lookupTable[dutyCh0]);
		break;
	}
	case TUNABLEWHITE:
	{
		if (isTwBipolar)
		{
			// 2-Wire tunable LEDs. Different polarity for each channel controlled by 4quadrant H-Brige
			float maxBt = actBrightness * 1023.0 / MAX_BRIGHTNESS / 3800.0;

			int dutyCh0 = round((actTemperature - 2700) * maxBt);
			int dutyCh1 = round((6500 - actTemperature) * maxBt);
#if defined(ESP32)
			ledc_set_duty_with_hpoint(LEDC_HIGH_SPEED_MODE, esp32LedCh[0], dutyCh0, 0);
			ledc_set_duty_with_hpoint(LEDC_HIGH_SPEED_MODE, esp32LedCh[1], dutyCh1, dutyCh0);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, esp32LedCh[0]);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, esp32LedCh[1]);
#elif defined(ESP8266)
			// TODO
			ledAnalogWrite(0, lookupTable[dutyCh0]);
			ledAnalogWrite(1, lookupTable[dutyCh1]);
#endif
		}
		else
		{
			int dutyCh0 = 0;
			int dutyCh1 = 0;

			if (!isTwTempCh)
			{
				dutyCh0 = round(min(2 * (actTemperature - 2700), 3800) / 3800.0 * actBrightness);
				dutyCh1 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * actBrightness);
			}
			else if (actBrightness > 0)
			{
				int dutyCh0 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * MAX_BRIGHTNESS);
				int dutyCh1 = actBrightness;
			}
			ledAnalogWrite(0, lookupTable[dutyCh0]);
			ledAnalogWrite(1, lookupTable[dutyCh1]);
		}
		break;
	}
	case RGB:
	{
		rgb_t _rgb;
		hsv2rgb(actHsv, _rgb);

		ledAnalogWrite(0, (int)_rgb.red * 1023 / 255);
		ledAnalogWrite(1, (int)_rgb.green * 1023 / 255);
		ledAnalogWrite(2, (int)_rgb.blue * 1023 / 255);
		break;
	}
	case RGBW:
	{
		rgb_t _rgb;
		// Die folgenden Berechnungen sollten bereits vor dem Fade durchgeführt werden, damit ein schönes Fading garantiert wird
		if (currentLightMode == MODE_RGB)
		{
			hsv2rgb(actHsv, _rgb);
		}
		else
		{
			kelvin2rgb(actTemperature, actBrightness, _rgb);
		}

		rgbw_t _rgbw;
		rgb2Rgbw(_rgb, _rgbw);

		ledAnalogWrite(0, (int)_rgbw.red * 1023 / 255);
		ledAnalogWrite(1, (int)_rgbw.green * 1023 / 255);
		ledAnalogWrite(2, (int)_rgbw.blue * 1023 / 255);
		ledAnalogWrite(3, (int)_rgbw.white * 1023 / 255);

		break;
	}
	case RGBCT:
		rgb_t _rgb;
		hsv2rgb(actHsv, _rgb);
		// Serial.printf("PWM IST: R=%3d,G=%3d,B=%3d H=%3d,S=%3d,V=%3d\n", _rgb.red, _rgb.green, _rgb.blue, actHsv.h, actHsv.s, actHsv.v);

		ledAnalogWrite(0, (int)_rgb.red * 1023 / 255);
		ledAnalogWrite(1, (int)_rgb.green * 1023 / 255);
		ledAnalogWrite(2, (int)_rgb.blue * 1023 / 255);

		int dutyCh0 = 0;
		int dutyCh1 = 0;

		if (!isTwTempCh)
		{
			dutyCh0 = round(min(2 * (actTemperature - 2700), 3800) / 3800.0 * (actBrightness - actHsv.v));
			dutyCh1 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * (actBrightness - actHsv.v));
		}
		else if (actBrightness > actHsv.v)
		{
			int dutyCh0 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * MAX_BRIGHTNESS);
			int dutyCh1 = actBrightness - actHsv.v;
		}
		ledAnalogWrite(3, lookupTable[dutyCh0]);
		ledAnalogWrite(4, lookupTable[dutyCh1]);
	}
}

void KnxLed::ledAnalogWrite(byte channel, int duty)
{
#if defined(ESP32)
	ledcWrite(esp32LedCh[channel], duty);
#elif defined(ESP8266)
	analogWrite(outputPins[channel], duty);
#endif
}

bool KnxLed::getSwitchState()
{
	return setpointBrightness > 0;
}

int KnxLed::getBrightness()
{
	return max(0, actBrightness);
}

int KnxLed::getTemperature()
{
	return actTemperature;
}

void KnxLed::registerStatusCallback(callbackBool *fctn)
{
	returnStatusFctn = fctn;
}

void KnxLed::registerBrightnessCallback(callbackInt *fctn)
{
	returnBrightnessFctn = fctn;
}

void KnxLed::registerTemperatureCallback(callbackInt *fctn)
{
	returnTemperatureFctn = fctn;
}

void KnxLed::registerColorRgbCallback(callbackRgb *fctn)
{
	returnColorRgbFctn = fctn;
}

void KnxLed::registerColorHsvCallback(callbackHsv *fctn)
{
	returnColorHsvFctn = fctn;
}

void KnxLed::initSwitchableLight(uint8_t switchPin)
{
	lightType = SWITCHABLE;
	outputPins[0] = switchPin;
	pinMode(outputPins[0], OUTPUT);
	initialized = true;
}

void KnxLed::initDimmableLight(uint8_t ledPin)
{
	lightType = DIMMABLE;
	outputPins[0] = ledPin;
	initOutputChannels(1);
}

void KnxLed::initTunableWhiteLight(uint8_t cwPin, uint8_t wwPin, __cctMode cctMode)
{
	lightType = TUNABLEWHITE;
	outputPins[0] = cwPin;
	outputPins[1] = wwPin;
	isTwBipolar = cctMode == bipolar;
	isTwTempCh = cctMode == tempChannel;
	initOutputChannels(2);
}

void KnxLed::initRgbLight(uint8_t rPin, uint8_t gPin, uint8_t bPin)
{
	lightType = RGB;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;
	initOutputChannels(3);
}

void KnxLed::initRgbwLight(uint8_t rPin, uint8_t gPin, uint8_t bPin, uint8_t wPin, rgb_t whiteLedRgbEquivalent)
{
	lightType = RGBW;
	currentLightMode = MODE_RGB;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;
	outputPins[3] = wPin;
	whiteRgbEquivalent = whiteLedRgbEquivalent;
	initOutputChannels(4);
}

void KnxLed::initRgbcctLight(uint8_t rPin, uint8_t gPin, uint8_t bPin, uint8_t cwPin, uint8_t wwPin, __cctMode cctMode)
{
	lightType = RGBCT;
	currentLightMode = MODE_RGB;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;
	outputPins[3] = cwPin;
	outputPins[4] = wwPin;
	isTwBipolar = cctMode == bipolar;
	isTwTempCh = cctMode == tempChannel;
	initOutputChannels(5);
}

// internal helper which will be called by init
void KnxLed::initOutputChannels(uint8_t usedChannels)
{
#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - usedChannels)
	{
		for (uint i = 0; i < usedChannels; i++)
		{
			esp32LedCh[i] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
			ledcSetup(esp32LedCh[i], pwmFrequency, pwmResolution);
			ledcAttachPin(outputPins[i], esp32LedCh[i]);
		}
	}
#elif defined(ESP8266)
	for (uint8_t i = 0; i < usedChannels; i++)
	{
		pinMode(outputPins[i], OUTPUT);
	}
	analogWriteRange(pwmResolution);
	analogWriteFreq(pwmFrequency);
#endif
	initialized = true;
}

void KnxLed::rgb2hsv(const rgb_t rgb, hsv_t &hsv)
{
	float r = rgb.red / 255.0f;
	float g = rgb.green / 255.0f;
	float b = rgb.blue / 255.0f;

	float h, s, v; // h:0-360.0, s:0.0-1.0, v:0.0-1.0

	float max = max_f(r, g, b);
	float min = min_f(r, g, b);

	v = max;

	if (max == 0.0f)
	{
		s = 0;
		h = 0;
	}
	else if (max - min == 0.0f)
	{
		s = 0;
		h = 0;
	}
	else
	{
		s = (max - min) / max;

		if (max == r)
		{
			h = 60 * ((g - b) / (max - min)) + 0;
		}
		else if (max == g)
		{
			h = 60 * ((b - r) / (max - min)) + 120;
		}
		else
		{
			h = 60 * ((r - g) / (max - min)) + 240;
		}
	}

	if (h < 0)
	{
		h += 360.0f;
	}
	else if (h >= 360)
	{
		h -= 360.0f;
	}

	hsv.h = round(h);		   // dst_h : 0-360
	hsv.s = round(s * 255.0f); // dst_s : 0-255
	hsv.v = round(v * 255.0f); // dst_v : 0-255
}

void KnxLed::hsv2rgb(const hsv_t hsv, rgb_t &rgb)
{
	float h = hsv.h;		  // 0-360
	float s = hsv.s / 255.0f; // 0.0-1.0
	float v = hsv.v / 255.0f; // 0.0-1.0

	float r = 0, g = 0, b = 0; // 0.0-1.0

	int hi = (int)(h / 60.0f) % 6;
	float f = (h / 60.0f) - hi;
	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
	float t = v * (1.0f - s * (1.0f - f));

	switch (hi)
	{
	case 0:
		r = v, g = t, b = p;
		break;
	case 1:
		r = q, g = v, b = p;
		break;
	case 2:
		r = p, g = v, b = t;
		break;
	case 3:
		r = p, g = q, b = v;
		break;
	case 4:
		r = t, g = p, b = v;
		break;
	case 5:
		r = v, g = p, b = q;
		break;
	}

	rgb.red = round(r * 255.0f);
	rgb.green = round(g * 255.0f);
	rgb.blue = round(b * 255.0f);
}

void KnxLed::kelvin2rgb(const uint16_t temperature, const uint8_t brightness, rgb_t &rgb)
{
	float red;
	float green;
	float blue;
	float temp = constrain(temperature, 500, 40000) / 100;

	// Calculate red
	if (temp <= 66)
	{
		red = 255;
	}
	else
	{
		red = 329.698727446 * pow(temp - 60, -0.1332047592);
	}

	// Calculate green
	if (temp < 66)
	{
		green = 99.4708025861 * log(temp) - 161.1195681661;
	}
	else
	{
		green = 288.1221695283 * pow(temp - 60, -0.0755148492);
	}

	// Calculate blue
	if (temp <= 19)
	{
		blue = 0;
	}
	else if (temp <= 66)
	{
		blue = 138.5177312231 * log(temp - 10) - 305.0447927307;
	}
	else
	{
		blue = 255;
	}

	red = red * brightness / 255;
	green = green * brightness / 255;
	blue = blue * brightness / 255;

	// Constrains values for 8 bit PWM
	rgb.red = constrain(red, 0, 255) + 0.5;
	rgb.green = constrain(green, 0, 255) + 0.5;
	rgb.blue = constrain(blue, 0, 255) + 0.5;
}

void KnxLed::rgb2Rgbw(const rgb_t rgb, rgbw_t &rgbw)
{
	// be to get the corresponding color value.
	double whiteValueForRed = rgb.red * 255.0 / whiteRgbEquivalent.red;
	double whiteValueForGreen = rgb.green * 255.0 / whiteRgbEquivalent.green;
	double whiteValueForBlue = rgb.blue * 255.0 / whiteRgbEquivalent.blue;

	// Set the white value to the highest it can be for the given color
	// (without over saturating any channel - thus the minimum of them).
	double minWhiteValue = min(whiteValueForRed,
							   min(whiteValueForGreen,
								   whiteValueForBlue));

	// The rest of the channels will just be the original value minus the
	// contribution by the white channel.
	rgbw.red = rgb.red - round(minWhiteValue * whiteRgbEquivalent.red / 255.0);
	rgbw.green = rgb.green - round(minWhiteValue * whiteRgbEquivalent.green / 255.0);
	rgbw.blue = rgb.blue - round(minWhiteValue * whiteRgbEquivalent.blue / 255.0);
	rgbw.white = constrain(minWhiteValue, 0, 255) + 0.5;
	// Serial.printf("RGB IN: R=%3d,G=%3d,B=%3d OUT R=%3d,G=%3d,B=%3d,W=%3d\n", rgb.red, rgb.green, rgb.blue, rgbw.red, rgbw.green, rgbw.blue, rgbw.white);

	double maxRgbIn = max(rgb.red,
						  max(rgb.green,
							  rgb.blue));
	double maxRgbOut = max(rgbw.red, max(rgbw.green,
										 max(rgbw.blue,
											 rgbw.white)));
	double factor = 0.0;
	if (maxRgbOut > 0)
	{
		factor = maxRgbIn / maxRgbOut;
	}
	rgbw.red = round(rgbw.red * factor);
	rgbw.green = round(rgbw.green * factor);
	rgbw.blue = round(rgbw.blue * factor);
	rgbw.white = round(rgbw.white * factor);
	// Serial.printf("RGB OUT2 R=%3d,G=%3d,B=%3d,W=%3d\n", rgbw.red, rgbw.green, rgbw.blue, rgbw.white);
}