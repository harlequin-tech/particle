#ifndef SLEEPER_H_
#define SLEEPER_H_

#define SLEEP_WAKE_DURATION   30
#define SLEEP_SLEEP_DURATION  60

//#include "../lib/PCF8523/src/PCF8523.h"
#include "ds3231.h"
#include "pcf.h"
#include "rtc.h"


class Sleeper {
    public:
        Sleeper(rtcType_e rtcType, uint8_t i2cAddr, uint16_t wakePin, uint16_t ledPin);
        void stop(void);
        void loop(uint32_t now=0);
	void enable(void) { _enabled = true; _lastSleep = 0; }
	void disable(void) { _enabled = false; }
        void woken(void) { _awake = true; }
        static void wakeup(void);

	RTC rtc;
        int init(void);         // reset and setup RTC chip
        // bool rtcSecondsIs(uint8_t seconds);
        void printTimestamp(DateTime now);
        void printTimestamp(void);
        void clear(void);

    private:
	bool _enabled = false;
	uint32_t _wakeDuration = SLEEP_WAKE_DURATION;		// seconds
	uint32_t _sleepDuration = SLEEP_SLEEP_DURATION;		// seconds
	uint32_t _lastSleep = 0;
	uint32_t _waitingToWake = 0;
	uint16_t _ledPin;
	uint16_t _wakePin;
        volatile bool _awake = false;
	bool _rtcEnabled = false;
	const char _daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
};

#endif
