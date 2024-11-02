#ifndef DS3231_H_
#define DS3231_H_

/*
 * ds3231.h
 *
 *
 */

#include "i2c.h"
#include "rtcalarm.h"

#include "datetime.h"
#include "ds3231_regs.h"

class DS3231 {
    public:
        DS3231(I2C &i2c) : _i2c(i2c) {};


        bool ready(void);
        void reset(void);
        bool exists(void);
        void dump(void);

        void set(DateTime dt);
        DateTime now();

        bool alarmTriggered(uint8_t alarmId); 
        int clearAlarm(uint8_t alarmId=0xFF);
        int enableAlarm(uint8_t alarmId);
        int disableAlarm(uint8_t alarmId=0xFF);
        int setCountdown(uint8_t alarmId, uint32_t seconds);
        int setAlarm(uint8_t alarmId, int8_t second, int8_t minute=-1, int8_t hour=-1, int8_t dayOfWeek=-1, int8_t day=-1);

        void enable32kHz(void);     // Turns the 32kHz output pin on
        void disable32kHz(void);    // Turns 32kHz output pin off
        void setFrequency(uint8_t fequency=DS_FREQ_1HZ);
        void enableSquareWave(uint8_t fequency=DS_FREQ_1HZ, bool batteryBacked=false);

        float getTemperature(); 

        I2C _i2c;

        // TBD replace
        void enableOscillator(bool battery, byte frequency, bool TF=true); 
                // turns oscillator on or off. True is on, false is off.
                // if battery is true, turns on even for battery-only operation,
                // otherwise turns off if Vcc is off.
                // frequency must be 0, 1, 2, or 3.
                // 0 = 1 Hz
                // 1 = 1.024 kHz
                // 2 = 4.096 kHz
                // 3 = 8.192 kHz (Default if frequency byte is out of range);
        void disableOscillator(bool battery, byte frequency);
        bool oscillatorCheck();;
                // Checks the status of the OSF (Oscillator Stop Flag);.
                // If this returns false, then the clock is probably not
                // giving you the correct time.
                // The OSF is cleared by function setSecond();.
                

    private:
        dateRaw_t _date;
        RtcAlarm _alarm[DS_ALARM_MAX+1];
        byte readControlByte(bool which);       // Read selected control byte: (0); reads 0x0e, (1) reads 0x0f
        uint8_t _hourFlags = 0;
        uint8_t _dayFlags = 0;

        // Write the selected control byte. 
        // which == false -> 0x0e, true->0x0f.
        void writeControlByte(byte control, bool which); 

};

#endif
