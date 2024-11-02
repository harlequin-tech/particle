/*
 *
 * rtc.cpp
 *
 * Copyright (c) 2020, All Rights Reserved.
 * Author: Darran Hunt
 */

#include <Arduino.h>
#include "rtc.h"
#include "i2c.h"

#define DEC_TO_BCD(val)    (uint8_t)(((((uint8_t)(val))/10) << 4) | (((uint8_t)(val)) % 10))
#define BCD_TO_DEC(val)    (uint8_t)(((((uint8_t)(val))>>4) * 10) | ((uint8_t)(val) & 0x0F))

/**********************************************************************
 *
 * RTC Alarm
 *
 */

RtcAlarm::RtcAlarm()
{
}

void RtcAlarm::setTrigger(uint8_t trigger)
{
    _trigger = trigger;

    _raw.setting.day = (_raw.setting.day & ~DS_TRIGGER) | ((trigger & DS_TRIGGER_DAY) ? 0: DS_TRIGGER);
    _raw.setting.hour = (_raw.setting.hour & ~DS_TRIGGER) | ((trigger & DS_TRIGGER_HOUR) ? 0: DS_TRIGGER);
    _raw.setting.minute = (_raw.setting.minute & ~DS_TRIGGER) | ((trigger & DS_TRIGGER_MINUTE) ? 0: DS_TRIGGER);
    _raw.setting.second = (_raw.setting.second & ~DS_TRIGGER) | ((trigger & DS_TRIGGER_SECOND) ? 0: DS_TRIGGER);
}

void RtcAlarm::set(int8_t second, int8_t minute, int8_t hour, int8_t dayOfWeek, int8_t day)
{
    uint8_t trigger = DS_TRIGGER_SECOND;

    _raw.setting.second = DEC_TO_BCD(second);
    if (minute >= 0) {
	_raw.setting.minute = DEC_TO_BCD(minute);
	trigger = DS_TRIGGER_MINUTE | DS_TRIGGER_SECOND;
    }
    if (hour >= 0) {
	_raw.setting.hour = DEC_TO_BCD(hour);
	trigger = DS_TRIGGER_HOUR | DS_TRIGGER_MINUTE | DS_TRIGGER_SECOND;
    }
    if (day >= 0) {
	// day wins if both day and dayOfWeek specified
	_raw.setting.day = DEC_TO_BCD(day);
	trigger = DS_TRIGGER_DAY | DS_TRIGGER_HOUR | DS_TRIGGER_MINUTE | DS_TRIGGER_SECOND;
    } else if (dayOfWeek >= 0) {
	_raw.setting.day = DEC_TO_BCD(day) | DS_DAY_OF_WEEK;
	trigger = DS_TRIGGER_DAY | DS_TRIGGER_HOUR | DS_TRIGGER_MINUTE | DS_TRIGGER_SECOND;
    }

    Serial1.printlnf("set(s=%u, m=%u, h=%u, wd=%u, d=%u)", second, minute, hour, dayOfWeek, day);
    Serial1.printlnf("_raw(s=0x%02x, m=0x%02x, h=0x%02x, d=0x%02x)", 
            _raw.data[0], _raw.data[1], _raw.data[2], _raw.data[3]);

    setTrigger(trigger);
    Serial1.printlnf("_trg(s=0x%02x, m=0x%02x, h=0x%02x, d=0x%02x)", 
            _raw.data[0], _raw.data[1], _raw.data[2], _raw.data[3]);
}

void RtcAlarm::enable(void)
{
    uint8_t ind;

    Serial1.printlnf("enable: alarmId %u", _alarmId);
    if (_alarmId > DS_ALARM_MAX) return;

    uint8_t startReg = (_alarmId == DS_ALARM_1) ?  DS_REG_A1_SECOND : DS_REG_A2_MINUTE;
    if (_alarmId == DS_ALARM_1) {
	ind = 0;
        startReg = DS_REG_A1_SECOND;
    } else {
	ind = 1;
        startReg = DS_REG_A2_MINUTE;
    }

    for (ind=0; ind<sizeof(_raw.data); ind++) {
        Serial1.printlnf("    %u: 0x%02x", startReg + ind, _raw.data[ind]);
    }

    if (_alarmId == DS_ALARM_1) {
        _i2c.write(DS_REG_A1_SECOND, 4, &_raw.data[0]);
    } else {
        _i2c.write(DS_REG_A2_MINUTE, 3, &_raw.data[1]);
    }

    // XXX add PCF
    _i2c.modify(DS_REG_CONTROL,
	    _alarmEnable[_alarmId]|DS_CONTROL_INTCN,
	    _alarmEnable[_alarmId]|DS_CONTROL_INTCN);
}

void RtcAlarm::disable(void)
{
    // XXX add PCF
    _i2c.modify(DS_REG_CONTROL, _alarmEnable[_alarmId], 0);
}

bool RtcAlarm::triggered(void)
{
    return (_i2c.read(DS_REG_STATUS) & _alarmEnable[_alarmId]) != 0;
}

uint8_t RtcAlarm::clear(void)
{
    Serial1.printlnf("RtcAlarm::clear() id %d -> 0x%x\n", _alarmId, _alarmEnable[_alarmId]);
    return _i2c.modify(DS_REG_STATUS, _alarmEnable[_alarmId], 0);
}


/**********************************************************************
 *
 * RTC
 *
 */

RTC::RTC(void)
{
}

int RTC::init(rtcType_e type, uint8_t i2cAddr)
{

    if (type <= RTC_NONE || type >= RTC_MAX) {
        return -1;
    }

    _type = type;
    _i2c.setAddr(i2cAddr);

    return 0;
}

void RTC::dump(void)
{
    switch (_type) {
    case RTC_DS3231:
        _ds.dump();
        break;

    case RTC_PCF8532:
        _pcf.dump();
        break;

    default:
        break;
    }
}

bool RTC::ready(void)
{
    switch (_type) {
    case RTC_DS3231:
        return _ds.ready();
        break;

    case RTC_PCF8532:
        return _pcf.ready();
        break;

    default:
        return false;
    }

    return false;
}

int RTC::reset(void)
{
    switch (_type) {
    case RTC_DS3231:
        _ds.reset();
        break;

    case RTC_PCF8532:
        _pcf.reset();
        break;

    default:
        return -1;
    }

    return 0;
}


void RTC::set(DateTime& dt)
{
    switch (_type) {
    case RTC_DS3231:
        _ds.set(dt);
        break;

    case RTC_PCF8532:
        _pcf.set(dt);
        break;

    default:
        break;
    }
}


DateTime RTC::now()
{
    switch (_type) {
    case RTC_DS3231:
        return _ds.now();

    case RTC_PCF8532:
        return _pcf.now();

    default:
        break;
    }

    DateTime none;
    return none;
}


int RTC::setAlarm(uint8_t alarmId, int8_t second, int8_t minute, int8_t hour, int8_t dayOfWeek, int8_t day)
{
    switch (_type) {
    case RTC_DS3231:
        _ds.setAlarm(alarmId, second, minute, hour, dayOfWeek, day);
        break;

    case RTC_PCF8532:
        _pcf.setAlarm(minute, hour, dayOfWeek, day);
        break;

    default:
        return -1;
    }

    return 0;
}

int RTC::setCountdown(uint8_t alarmId, uint32_t seconds)
{
    int res=-1;

    if (_type == RTC_PCF8532 && seconds < 60) {
        // only supports minute alarm
        seconds = 60;
    }

    DateTime dt = now() + TimeSpan(seconds);

    res = setAlarm(alarmId, dt.second(), dt.minute(), dt.hour());

    char buf[64];
    char buf2[64];
    Serial1.printlnf("%s: setCountdown(): alarm set for %s\n",
            now().str(buf2, sizeof buf2),
            dt.str(buf, sizeof buf ));

    if (res >= 0) {
        res = enableAlarm(alarmId);
    }

    return res;
}


int RTC::clearAlarm(uint8_t alarmId)
{
    switch (_type) {
    case RTC_DS3231:
        return _ds.clearAlarm(alarmId);
        break;

    case RTC_PCF8532:
        return _pcf.clearAlarm(alarmId);
        break;

    default:
        break;
    }

    return -1;
}

int RTC::enableAlarm(uint8_t alarmId)
{
    clearAlarm(alarmId);

    switch (_type) {
    case RTC_DS3231:
        return _ds.enableAlarm(alarmId);
        break;

    case RTC_PCF8532:
        return _pcf.enableAlarm(alarmId);
        break;

    default:
        break;
    }

    return -1;
}


int RTC::disableAlarm(uint8_t alarmId)
{
    switch (_type) {
    case RTC_DS3231:
        return _ds.disableAlarm(alarmId);
        break;

    case RTC_PCF8532:
        return _pcf.disableAlarm(alarmId);
        break;

    default:
        break;
    }

    return -1;
}
