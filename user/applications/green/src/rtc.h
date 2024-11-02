#ifndef RTC_H_
#define RTC_H_

/*
 * rtc.h
 *
 */

#include <Arduino.h>

#include "i2c.h"
#include "pcf.h"
#include "ds3231.h"
#include "ds3231_regs.h"

#include "rtcalarm.h"

typedef enum {
    RTC_NONE    = 0,
    RTC_DS3231  = 1,
    RTC_PCF8532 = 2,

    // put new device IDs above this
    RTC_MAX
} rtcType_e;

class RTC {
    public:
        RTC(void);
        int init(rtcType_e rtcType, uint8_t i2cAddr);

        bool ready(void);               // RTC is ready and responding on I2C
        int reset(void);
        void set(DateTime& dt);
        DateTime now();

        int setAlarm(uint8_t alarmId, int8_t second, int8_t minute=-1, int8_t hour=-1, int8_t dayOfWeek=-1, int8_t day=-1);
        int setCountdown(uint8_t alarmId, uint32_t seconds);
        int clearAlarm(uint8_t alarmId=0xFF);
        int enableAlarm(uint8_t alarmId=0);
        int disableAlarm(uint8_t alarmId=0);

        alarmRaw_t *getRaw(void) { return &_raw; }

        void dump(void);

        I2C _i2c;
        DS3231 _ds = DS3231(_i2c);
        PCF8523 _pcf = PCF8523(_i2c);
    private:
        alarmRaw_t _raw;
        uint8_t _type;
};

#endif
