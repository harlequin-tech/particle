/*
 * pcf.h
 *
 */

#ifndef PCF_H_
#define PCF_H_

#include <Arduino.h>
#include <Wire.h>

#include "i2c.h"

#include "datetime.h"
#include "pcf8523_regs.h"

class PCF8523 {
    public:
        PCF8523(I2C &i2c) : _i2c(i2c) {};

        void     set(DateTime& dt);
        DateTime now();

        bool ready(void);
        void reset(void);
        void clearAll(void);
        void dump(void);
        void disableClockout(void);

        void setAlarm(int8_t minute, int8_t hour, int8_t day, int8_t dayOfWeek);
        int  enableAlarm(uint8_t alarmId=0);
        int  disableAlarm(uint8_t alarmId=0);
        int  clearAlarm(uint8_t alarmId=0xFF);
        void setDeviceAddr(uint8_t addr) { _i2c.setAddr(addr); }

        void enableAlarmInterrupt(uint8_t alarmId);
        void disableAlarmInterrupt(uint8_t alarmId);

        I2C &_i2c;
    private:
        alarmRaw_t _raw;
        dateRaw_t _date;
        uint8_t _alarmId = 0;
};

#endif
