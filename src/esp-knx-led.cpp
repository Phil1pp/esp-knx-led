#include "esp-knx-led.h"
#if defined(ESP32)
byte nextEsp32LedChannel = LEDC_CHANNEL_0; // next available LED channel for ESP32
#endif

//lookup table for logarithmic dimming curve
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
	if (!initialized)
		return;

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
	case RGBWW:
		// TODO;
		break;
	}
}

void KnxLed::setBrightness(int brightness)
{
	setBrightness(brightness, true);
}

void KnxLed::setBrightness(int brightness, bool saveValue)
{
	if (!initialized)
		return;

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
	}
}

void KnxLed::setTemperature(int temperature)
{
	if (!initialized || !(lightType == TUNABLEWHITE || lightType == RGBW || lightType == RGBWW))
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
	if (setpointBrightness > actBrightness)
	{
		actBrightness++;
		updatePwm = true;
	}
	else if (setpointBrightness < actBrightness)
	{
		actBrightness--;
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
		if (!isTwBipolar)
		{
			// Dimming curve adapted for maximum brightness: CW ^ WW
			int dutyCh0 = round(min(2 * (actTemperature - 2700), 3800) / 3800.0 * actBrightness);
			int dutyCh1 = round(min(2 * (6500 - actTemperature), 3800) / 3800.0 * actBrightness);
			ledAnalogWrite(0, lookupTable[dutyCh0]);
			ledAnalogWrite(1, lookupTable[dutyCh1]);
		}
		else
		{
			//2-Wire tunable LEDs. Different polarity for each channel controlled by 4quadrant H-Brige
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
		break;
	}
	case RGB:
	case RGBW:
	case RGBWW:
		// TODO;
		break;
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

void KnxLed::initSwitchableLight(byte switchPin)
{
	lightType = SWITCHABLE;
	outputPins[0] = switchPin;
	pinMode(outputPins[0], OUTPUT);
	initialized = true;
}

void KnxLed::initDimmableLight(byte ledPin)
{
	lightType = DIMMABLE;
	outputPins[0] = ledPin;

#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - 1)
	{
		esp32LedCh[0] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		ledcSetup(esp32LedCh[0], pwmFrequency, pwmResolution);
		ledcAttachPin(outputPins[0], esp32LedCh[0]);
		initialized = true;
	}
#elif defined(ESP8266)
	pinMode(outputPins[0], OUTPUT);
	analogWriteRange(pwmResolution);
	analogWriteFreq(pwmFrequency);
	initialized = true;
#endif
}

void KnxLed::initTunableWhiteLight(byte cwPin, byte wwPin)
{
	lightType = TUNABLEWHITE;
	outputPins[0] = cwPin;
	outputPins[1] = wwPin;

#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - 2)
	{
		esp32LedCh[0] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[1] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		ledcSetup(esp32LedCh[0], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[1], pwmFrequency, pwmResolution);
		ledcAttachPin(outputPins[0], esp32LedCh[0]);
		ledcAttachPin(outputPins[1], esp32LedCh[1]);
		initialized = true;
	}
#elif defined(ESP8266)
	pinMode(outputPins[0], OUTPUT);
	pinMode(outputPins[1], OUTPUT);
	analogWriteRange(pwmResolution);
	analogWriteFreq(pwmFrequency);
	initialized = true;
#endif
}

void KnxLed::initRgbLight(byte rPin, byte gPin, byte bPin)
{
	lightType = RGB;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;

#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - 3)
	{
		esp32LedCh[0] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[1] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[2] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		ledcSetup(esp32LedCh[0], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[1], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[2], pwmFrequency, pwmResolution);
		ledcAttachPin(outputPins[0], esp32LedCh[0]);
		ledcAttachPin(outputPins[1], esp32LedCh[1]);
		ledcAttachPin(outputPins[2], esp32LedCh[2]);
		initialized = true;
	}
#elif defined(ESP8266)
	pinMode(outputPins[0], OUTPUT);
	pinMode(outputPins[1], OUTPUT);
	pinMode(outputPins[2], OUTPUT);
	analogWriteRange(pwmResolution);
	analogWriteFreq(pwmFrequency);
	initialized = true;
#endif
}

void KnxLed::initRgbwLight(byte rPin, byte gPin, byte bPin, byte wPin)
{
	lightType = RGBW;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;
	outputPins[3] = wPin;

#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - 4)
	{
		esp32LedCh[0] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[1] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[2] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[3] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		ledcSetup(esp32LedCh[0], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[1], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[2], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[3], pwmFrequency, pwmResolution);
		ledcAttachPin(outputPins[0], esp32LedCh[0]);
		ledcAttachPin(outputPins[1], esp32LedCh[1]);
		ledcAttachPin(outputPins[2], esp32LedCh[2]);
		ledcAttachPin(outputPins[3], esp32LedCh[3]);
		initialized = true;
	}
#elif defined(ESP8266)
	pinMode(outputPins[0], OUTPUT);
	pinMode(outputPins[1], OUTPUT);
	pinMode(outputPins[2], OUTPUT);
	pinMode(outputPins[3], OUTPUT);
	analogWriteRange(pwmResolution);
	analogWriteFreq(pwmFrequency);
	initialized = true;
#endif
}

void KnxLed::initRgbwwLight(byte rPin, byte gPin, byte bPin, byte cwPin, byte wwPin)
{
	lightType = RGBWW;
	outputPins[0] = rPin;
	outputPins[1] = gPin;
	outputPins[2] = bPin;
	outputPins[3] = cwPin;
	outputPins[4] = wwPin;

#if defined(ESP32)
	if (nextEsp32LedChannel <= LEDC_CHANNEL_MAX - 5)
	{
		esp32LedCh[0] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[1] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[2] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[3] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		esp32LedCh[4] = static_cast<ledc_channel_t>(nextEsp32LedChannel++);
		ledcSetup(esp32LedCh[0], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[1], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[2], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[3], pwmFrequency, pwmResolution);
		ledcSetup(esp32LedCh[4], pwmFrequency, pwmResolution);
		ledcAttachPin(outputPins[0], esp32LedCh[0]);
		ledcAttachPin(outputPins[1], esp32LedCh[1]);
		ledcAttachPin(outputPins[2], esp32LedCh[2]);
		ledcAttachPin(outputPins[3], esp32LedCh[3]);
		ledcAttachPin(outputPins[4], esp32LedCh[4]);
		initialized = true;
	}
#elif defined(ESP8266)
	pinMode(outputPins[0], OUTPUT);
	pinMode(outputPins[1], OUTPUT);
	pinMode(outputPins[2], OUTPUT);
	pinMode(outputPins[3], OUTPUT);
	pinMode(outputPins[4], OUTPUT);
	analogWriteFreq(pwmFrequency);
	initialized = true;
#endif
}

//For 2-Wire tunable LEDs with ifferent polarity for each channel
void KnxLed::setupTwBipolar()
{
	isTwBipolar = true;
}

//For tunable LED bulbs where temperature and brightness are controlled by 2 channels
void KnxLed::setupTwTempCh()
{
	isTwTempCh = true;
}