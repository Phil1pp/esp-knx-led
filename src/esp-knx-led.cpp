#include "esp-knx-led.h"
#if defined(ESP32)
byte nextEsp32LedChannel = LEDC_CHANNEL_0; // next available LED channel for ESP32
#endif

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
	if (lightType == RGBW)
	{
		rgb_t _rgb;
		hsv_t _hsv;
		kelvin2rgb(setpointTemperature, setpointBrightness, _rgb);
		rgb2hsv(_rgb, _hsv);
		setpointHsv = _hsv;
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

	uint8_t diffH = abs(setpointHsv.h - actHsv.h);
	if (diffH > 0)
	{
		bool do360overflow = diffH > 128;
		if ((setpointHsv.h > actHsv.h) != do360overflow)
		{
			actHsv.h = (actHsv.h + 1) % 256;
		}
		else
		{
			actHsv.h = (actHsv.h + 255) % 256;
		}
		updatePwm = true;
	}

	if (diffH > 43 && (setpointHsv.s - actHsv.s) < diffH && actHsv.s > 1)
	{
		actHsv.s -= 2;
		updatePwm = true;
	}
	else if (setpointHsv.s != actHsv.s)
	{
		actHsv.s += setpointHsv.s > actHsv.s ? 1 : -1;
		updatePwm = true;
	}

	if (lightType == RGBCT && currentLightMode == MODE_CCT)
	{
		if (actHsv.v > 0)
		{
			actHsv.v -= 1;
			updatePwm = true;
		}
	}
	else
	{
		if (setpointBrightness != actHsv.v)
		{
			actHsv.v += setpointBrightness > actHsv.v ? 1 : -1;
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
				dutyCh0 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * MAX_BRIGHTNESS);
				dutyCh1 = actBrightness;
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

		ledAnalogWrite(0, lookupTable[_rgb.red]);
		ledAnalogWrite(1, lookupTable[_rgb.green]);
		ledAnalogWrite(2, lookupTable[_rgb.blue]);
		break;
	}
	case RGBW:
	{
		rgb_t _rgb;
		hsv2rgb(actHsv, _rgb);
		uint8_t white = rgb2White(_rgb);

		ledAnalogWrite(0, lookupTable[_rgb.red]);
		ledAnalogWrite(1, lookupTable[_rgb.green]);
		ledAnalogWrite(2, lookupTable[_rgb.blue]);
		ledAnalogWrite(3, lookupTable[white]);
		
		/*rgbw_t _rgbw;
		rgb2Rgbw(_rgb, _rgbw);

		ledAnalogWrite(0, lookupTable[_rgbw.red]);
		ledAnalogWrite(1, lookupTable[_rgbw.green]);
		ledAnalogWrite(2, lookupTable[_rgbw.blue]);
		ledAnalogWrite(3, lookupTable[_rgbw.white]);*/

		break;
	}
	case RGBCT:
		rgb_t _rgb;
		hsv2rgb(actHsv, _rgb);
		// Serial.printf("PWM IST: R=%3d,G=%3d,B=%3d H=%3d,S=%3d,V=%3d\n", _rgb.red, _rgb.green, _rgb.blue, actHsv.h, actHsv.s, actHsv.v);

		ledAnalogWrite(0, lookupTable[_rgb.red]);
		ledAnalogWrite(1, lookupTable[_rgb.green]);
		ledAnalogWrite(2, lookupTable[_rgb.blue]);

		int dutyCh0 = 0;
		int dutyCh1 = 0;

		if (!isTwTempCh)
		{
			dutyCh0 = round(min(2 * (actTemperature - 2700), 3800) / 3800.0 * (actBrightness - actHsv.v));
			dutyCh1 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * (actBrightness - actHsv.v));
		}
		else if (actBrightness > actHsv.v)
		{
			dutyCh0 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * MAX_BRIGHTNESS);
			dutyCh1 = actBrightness - actHsv.v;
		}
		ledAnalogWrite(3, lookupTable[dutyCh0]);
		ledAnalogWrite(4, lookupTable[dutyCh1]);
	}
}

void KnxLed::ledAnalogWrite(byte channel, uint16_t duty)
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

	float max = max_f(r, g, b);
	float min = min_f(r, g, b);
	float d = max - min;

	float h = 0;
	float s = max == 0 ? 0 : d / max;
	float v = max;

	if (max != min)
	{
		if (max == r)
		{
			h = (g - b) / d + (g < b ? 6 : 0);
		}
		else if (max == g)
		{
			h = (b - r) / d + 2;
		}
		else
		{
			h = (r - g) / d + 4;
		}
		h /= 6;
	}

	hsv.h = round(h * 255.0f);
	hsv.s = round(s * 255.0f);
	hsv.v = round(v * 255.0f);
}

void KnxLed::hsv2rgb(const hsv_t hsv, rgb_t &rgb)
{
	float h = hsv.h / 255.0f;
	float s = hsv.s / 255.0f;
	float v = hsv.v / 255.0f;

	float r = 0, g = 0, b = 0;

	int i = floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	switch (i % 6)
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

uint8_t KnxLed::rgb2White(const rgb_t rgb)
{
	// Set the white value to the highest it can be for the given color
	// (without over saturating any channel - thus the minimum of them).
	float minWhiteValue = min_f(rgb.red * 255.0 / whiteRgbEquivalent.red, rgb.green * 255.0 / whiteRgbEquivalent.green, rgb.blue * 255.0 / whiteRgbEquivalent.blue);

	return constrain(minWhiteValue, 0, 255) + 0.5;
}