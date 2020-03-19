#ifndef RTCALARM_H_
#define RTCALARM_H_


#include "datetime.h"

#define RTC_ALARM_MAX   5

/**
 * RtcAlarm
 *
 * Use: set the alarmId, set the alarm time, then enable the alarm.
 */
class RtcAlarm {
    public:
        RtcAlarm();
        void init(uint8_t *enable, uint8_t count);
        alarmRaw_t *getRaw(void) { return &_raw; }
        void set(int8_t second=-1, int8_t minute=-1, int8_t hour=-1, int8_t dayOfWeek=-1, int8_t day=-1);
        void setTrigger(uint8_t trigger);
        void setId(uint8_t alarmId) { _alarmId = alarmId; }
        void enable(void);
        void disable(void);
        bool triggered(void);
        uint8_t clear(void);
        void setDeviceAddr(uint8_t addr) { _i2c.setAddr(addr); }
        void reset(void);

        I2C _i2c;
    private:
        alarmRaw_t _raw;
        uint8_t _alarmId = 0;
        uint8_t _trigger = 0;
        uint8_t _alarmEnable[RTC_ALARM_MAX];
        //uint8_t _alarmEnable[RTC_ALARM_MAX] = { DS_CONTROL_A1IE, DS_CONTROL_A2IE };
};

#endif
