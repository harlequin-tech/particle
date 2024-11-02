/***********************************************************************
 * pcf.cpp
 *
 * Copyright (c) 2020
 * All Rights Reserved.
 *
 * Author: Darran Hunt
 */

#include "pcf.h"

#define DEC_TO_BCD(val)    (uint8_t)(((((uint8_t)(val))/10) << 4) | (((uint8_t)(val)) % 10))
#define BCD_TO_DEC(val)    (uint8_t)(((((uint8_t)(val))>>4) * 10) + ((uint8_t)(val) & 0x0F))

//#define BCD_TO_DEC(val)   (uint8_t)((val) - 6 * (val >> 4))

//static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
//static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

void PCF8523::reset(void)
{
    disableClockout();
    disableAlarm();
    clearAll();
}

bool PCF8523::ready(void)
{
    return _i2c.read(PCF_REG_SECONDS) != 0xFF;
}

void PCF8523::dump(void)
{
    uint8_t data[19];
    _i2c.read(0, sizeof(data), data);
    Serial1.println();
    Serial1.printlnf("PCF8523 i2c addr: 0x%02x", _i2c.getAddr());
    Serial1.println("PCF8523 register contents");
    for (uint8_t ind=0; ind<sizeof(data); ind++) {
        Serial1.printlnf("%02x: %02x", ind, data[ind]);
    }
}


void PCF8523::setAlarm(int8_t minute, int8_t hour, int8_t day, int8_t dayOfWeek) 
{
    Serial1.printlnf("PCF8523:setAlarm(min=%d, hour=%d, day=%d, dayOfWeek=%d)",
            minute, hour, day, dayOfWeek);
    _raw.setting.minute    = minute < 0    ? PCF_ALARM_DISABLE : DEC_TO_BCD(minute);
    _raw.setting.hour      = hour < 0      ? PCF_ALARM_DISABLE : DEC_TO_BCD(hour);
    _raw.setting.day       = dayOfWeek < 0 ? PCF_ALARM_DISABLE : DEC_TO_BCD(day);
    _raw.setting.dayAlt    = PCF_ALARM_DISABLE;
}


void PCF8523::disableClockout(void)
{
    _i2c.modify(PCF_REG_TMR_CLKOUT_CTRL, PCF_CLKOUT_COF, 0xFF);
}


int PCF8523::disableAlarm(uint8_t alarmId)
{
    uint8_t data[4] = {
        PCF_ALARM_DISABLE,
        PCF_ALARM_DISABLE,
        PCF_ALARM_DISABLE,
        PCF_ALARM_DISABLE
    };
    _i2c.write(PCF_REG_MINUTE_ALARM, sizeof(data), &data[0]);

    disableAlarmInterrupt(alarmId);

    return 0;
}

void PCF8523::clearAll(void)
{
    _i2c.write(PCF_REG_CONTROL_2, 0);
}

/**
 *
 * returns 1 if alarm was raised.
 */
int PCF8523::clearAlarm(uint8_t alarmId) 
{
    // only one alarm
    uint8_t rc2 = _i2c.read(PCF_REG_CONTROL_2) & (PCF_CONTROL_2_SF_BIT | PCF_CONTROL_2_AF_BIT);

    _i2c.modify(PCF_REG_CONTROL_2, PCF_CONTROL_2_SF_BIT | PCF_CONTROL_2_AF_BIT, 0);

    return rc2 != 0;
}

void PCF8523::enableAlarmInterrupt(uint8_t alarmId)
{
    _i2c.modify(PCF_REG_CONTROL_1, PCF_CONTROL_1_AIE_BIT, PCF_CONTROL_1_AIE_BIT);
}

void PCF8523::disableAlarmInterrupt(uint8_t alarmId)
{
    _i2c.modify(PCF_REG_CONTROL_1, PCF_CONTROL_1_AIE_BIT, 0);
}


int PCF8523::enableAlarm(uint8_t alarmId)
{
    Serial1.printlnf("enable: alarmId %u", _alarmId);
    if (_alarmId > PCF_ALARM_MAX) return -1;

    uint8_t startReg = PCF_REG_MINUTE_ALARM;

    for (uint8_t ind=0; ind<sizeof(_raw.data); ind++) {
        Serial1.printlnf("    %u: 0x%02x", startReg + ind, _raw.data[ind]);
    }

    // start from 1 as seconds is not used on PCF
    _i2c.write(PCF_REG_MINUTE_ALARM, sizeof(_raw.data) - 1, &_raw.data[1]);

    enableAlarmInterrupt(alarmId);

    return 0;
}

DateTime PCF8523::now() 
{
    _i2c.read(PCF_REG_SECONDS, sizeof(_date.data), &_date.data[0]);

    _date.value.second &= 0x7F;

#if 0
    uint8_t mm = bcd2bin(Wire.read());
    uint8_t hh = bcd2bin(Wire.read());
    uint8_t d = bcd2bin(Wire.read());
    Wire.read();
    uint8_t m = bcd2bin(Wire.read());
    uint16_t y = bcd2bin(Wire.read()) + 2000;
#endif

    return DateTime(BCD_TO_DEC(_date.value.year) + 2000,
                    BCD_TO_DEC(_date.value.month),
                    BCD_TO_DEC(_date.value.day),
                    BCD_TO_DEC(_date.value.hour),
                    BCD_TO_DEC(_date.value.minute),
                    BCD_TO_DEC(_date.value.second));
}


void PCF8523::set(DateTime& dt) 
{
    char buf[32];

#if 0
    WIRE.beginTransmission(PCF8523_ADDRESS);
    WIRE._I2C_WRITE(0x03);
    WIRE._I2C_WRITE(bin2bcd(dt.second()));
    WIRE._I2C_WRITE(bin2bcd(dt.minute()));
    WIRE._I2C_WRITE(bin2bcd(dt.hour()));
    WIRE._I2C_WRITE(bin2bcd(dt.day()));
    WIRE._I2C_WRITE(bin2bcd(0));
    WIRE._I2C_WRITE(bin2bcd(dt.month()));
    WIRE._I2C_WRITE(bin2bcd(dt.year() - 2000));
    WIRE._I2C_WRITE(0);
    WIRE.endTransmission();
#endif

#if 0
    uint8_t data[7];
    data[0] = DEC_TO_BCD(dt.second());
    data[1] = DEC_TO_BCD(dt.minute());
    data[2] = DEC_TO_BCD(dt.hour());
    data[3] = DEC_TO_BCD(dt.day());
    data[4] = 0;      // day of week
    data[5] = DEC_TO_BCD(dt.month());
    data[6] = DEC_TO_BCD(dt.year()-2000);
#endif

    uint8_t data[7] = {
       DEC_TO_BCD(dt.second()),
       DEC_TO_BCD(dt.minute()),
       DEC_TO_BCD(dt.hour()),
       DEC_TO_BCD(dt.day()),
       0,      // day of week
       DEC_TO_BCD(dt.month()),
       DEC_TO_BCD(dt.year()-2000) };


    Serial1.printlnf("PCF8523::set(%s)", dt.str(buf, sizeof(buf)));

    for (uint8_t ind=0; ind<sizeof(data); ind++) {
        Serial1.printlnf("  %02u: 0x%02x", PCF_REG_SECONDS + ind, data[ind]);
    }
    
    _i2c.write(PCF_REG_SECONDS, sizeof(data), data);
}
