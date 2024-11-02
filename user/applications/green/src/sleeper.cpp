/**
 * Module: sleeper.c
 */

#include <Particle.h>
#include "sleeper.h"

static volatile bool _woken;


Sleeper::Sleeper(rtcType_e rtcType, uint8_t i2cAddr, uint16_t wakePin, uint16_t ledPin)
{
    _ledPin = ledPin;
    _wakePin = wakePin;
    _woken = false;
    rtc.init(rtcType, i2cAddr);
    //rtc.dump();

    pinMode(_wakePin, INPUT_PULLUP);
    //pinMode(_wakePin, INPUT);
    attachInterrupt(_wakePin, wakeup, RISING);

    _rtcEnabled = true;
}


void Sleeper::wakeup(void)
{
    _woken = true;
}


void Sleeper::printTimestamp(DateTime now)
{ 
    now = now + TimeSpan(13*60*60);

    char buf[32];
    Serial.println(now.str(buf, sizeof(buf)));
}


void Sleeper::printTimestamp(void)
{
    printTimestamp(rtc.now());
}


int Sleeper::init(void) 
{
    Serial.println("Initialising RTC.");
    rtc.reset();
    printTimestamp();
    Serial.println();

    return 0;
}

void Sleeper::clear(void)
{
    if (_rtcEnabled) {
        rtc.clearAlarm(0);
    }
}

void Sleeper::loop(uint32_t now)
{
    if (!_enabled) return;

    if (now == 0) {
	now = millis();
    }

    if (_lastSleep == 0) {
        _lastSleep = now;
    }

    if (_woken) {
        _woken = false;
        if (_rtcEnabled) {
            printTimestamp();
            uint8_t alarms = rtc.clearAlarm(0);
            Serial.printf(": woke up. Alarms=0x%x\n", alarms);
        }
        _waitingToWake = 0;
    }

    if (!_waitingToWake) {
        if ((now - _lastSleep) >= _wakeDuration) {
            DateTime dt = rtc.now();
            char buf[64];
	    if (_rtcEnabled) {
		Serial.printlnf(": Set wakeup alarm for %u seconds", _sleepDuration);
		rtc.setCountdown(0, _sleepDuration);
                Serial.printlnf("%s: Sleeping until RTC wakeup", dt.str(buf, sizeof(buf)));
                rtc.dump();
#if 0
                uint32_t last = millis();
                uint32_t now = 0;
                uint8_t wakePin = 1;
                do {
                    wakePin = digitalRead(D8);
                    now = millis();
                    if ((now - last) >= 5000) {
                        dt = rtc.now();
                        Serial.printlnf("%s: D8=%u. Waiting for D8 -> 0", dt.str(buf, sizeof(buf)), wakePin);
                        last = now;
                        rtc.dump();
                    }
                } while (digitalRead(D8) != 0);
                _lastSleep = millis();
                _waitingToWake = 0;
                dt = rtc.now();
                Serial.printlnf("%s: D8 -> 0, ending sleep", dt.str(buf, sizeof(buf)));
                rtc.dump();
                rtc.clearAlarm(0);
                Serial.println("--- cleared alarm");
                return;
#endif
	    } else {
                Serial.printlnf("%s: sleeping",  dt.str(buf, sizeof(buf)));
            }

            digitalWrite(_ledPin, LOW);
            led_set_update_enabled(0, nullptr); // Disable background LED updates
            LED_Off(LED_RGB);
            _waitingToWake = millis();
            System.sleep(SLEEP_MODE_DEEP, 0);
        }
    } else {
        if ((now - _waitingToWake) >= 65000) {
            Serial.println("Failed to get wakeup interrupt within 65 seconds");
            _waitingToWake = 0;
	    if (_rtcEnabled) {
		uint8_t alarms = rtc.clearAlarm(0);
		if (alarms != 0) {
		    Serial.println("register: Alarm was generated. Id=%x");
		} else {
		    Serial.println("register: Alarms were not active.");
		}
	    }
        }
        _lastSleep = now;
    }
}
