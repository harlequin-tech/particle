/*
 * ds3231.cpp
 *
 * DS3231 Real Time Clock library
*/

#include "ds3231.h"

#define DEC_TO_BCD(val)    (uint8_t)(((((uint8_t)(val))/10) << 4) | (((uint8_t)(val)) % 10))
#define BCD_TO_DEC(val)    (uint8_t)(((((uint8_t)(val))>>4) * 10) | ((uint8_t)(val) & 0x0F))

void DS3231::reset(void)
{
    disable32kHz();
    clearAlarm();
    disableAlarm();
}

bool DS3231::ready(void)
{
    return _i2c.read(DS_REG_CURRENT_SECOND) != 0xFF;
}

DateTime DS3231::now() 
{
    _i2c.read(DS_REG_CURRENT_SECOND, sizeof(_date.data), &_date.data[0]);

    _date.value.second &= 0x7F;
    return DateTime(_date.value.year + 2000,
                    _date.value.month,
                    _date.value.day,
                    _date.value.hour,
                    _date.value.minute,
                    _date.value.second);
}


void DS3231::set(DateTime dt)
{
    uint8_t data[7];
    data[0] = DEC_TO_BCD(dt.second());
    data[1] = DEC_TO_BCD(dt.minute());
    data[2] = DEC_TO_BCD(dt.hour()) | _hourFlags;
        //(int)(((uint8_t)(((signed char)(((int)(dt.DateTime::hour() / 10u)) << 4)) | ((signed char)(dt.DateTime::hour() % 10u)))) | ((DS3231*)this)->DS3231::_hourFlags)
    data[3] = 0;      // day of week
    data[4] = DEC_TO_BCD(dt.day()) | _dayFlags;
    data[5] = DEC_TO_BCD(dt.month());
    data[6] = DEC_TO_BCD(dt.year()-2000);
    char buf[32];

    Serial.printlnf("DS3231::set(%s)", dt.str(buf, sizeof(buf)));
    for (uint8_t ind=0; ind<sizeof(data); ind++) {
        Serial.printlnf("  %02u: 0x%02x", ind, data[ind]);
    }
    
    _i2c.write(DS_REG_CURRENT_SECOND, sizeof(data), data);
}

float DS3231::getTemperature() 
{
    uint8_t data[2];
    // temp registers (11h-12h) get updated automatically every 64s
    _i2c.read(DS_REG_TEMP_MSB, 2, data);

    return (data[0] << 8 | (uint16_t)data[1] >> 6) / 4.0;
}

void DS3231::enableOscillator(bool battery, byte frequency, bool TF) 
{
    // turns oscillator on or off. True is on, false is off.
    // if battery is true, turns on even for battery-only operation,
    // otherwise turns off if Vcc is off.
    // frequency must be 0, 1, 2, or 3.
    // 0 = 1 Hz
    // 1 = 1.024 kHz
    // 2 = 4.096 kHz
    // 3 = 8.192 kHz (Default if frequency byte is out of range)
    if (frequency > 3) frequency = 3;
    // read control byte in, but zero out current state of RS2 and RS1.
    byte temp_buffer = readControlByte(0) & 0b11100111;
    if (battery) {
            // turn on BBSQW flag
            temp_buffer = temp_buffer | 0b01000000;
    } else {
            // turn off BBSQW flag
            temp_buffer = temp_buffer & 0b10111111;
    }
    if (TF) {
            // set ~EOSC to 0 and INTCN to zero.
            temp_buffer = temp_buffer & 0b01111011;
    } else {
            // set ~EOSC to 1, leave INTCN as is.
            temp_buffer = temp_buffer | 0b10000000;
    }
    // shift frequency into bits 3 and 4 and set.
    frequency = frequency << 3;
    temp_buffer = temp_buffer | frequency;
    // And write the control bits
    writeControlByte(temp_buffer, 0);
}

void DS3231::disableOscillator(bool battery, byte frequency)
{
    enableOscillator(battery, frequency, false);
}

void DS3231::enable32kHz(void)
{
    _i2c.modify(DS_REG_STATUS, DS_STATUS_32KHZ, DS_STATUS_32KHZ);
}

void DS3231::disable32kHz(void) 
{
    _i2c.modify(DS_REG_STATUS, DS_STATUS_32KHZ, 0);
}


bool DS3231::oscillatorCheck() {
	// Returns false if the oscillator has been off for some reason.
	// If this is the case, the time is probably not correct.
	byte temp_buffer = readControlByte(1);
	bool result = true;
	if (temp_buffer & 0b10000000) {
		// Oscillator Stop Flag (OSF) is set, so return false.
		result = false;
	}
	return result;
}

byte DS3231::readControlByte(bool which) 
{
	// Read selected control byte
	// first byte (0) is 0x0e, second (1) is 0x0f
	if (which) {
            return _i2c.read(0x0f);
	} else {
            return _i2c.read(0x0e);
	}
}

void DS3231::writeControlByte(byte control, bool which) {
	// Write the selected control byte.
	// which=false -> 0x0e, true->0x0f.
	if (which) {
            _i2c.write(0x0f, control);
	} else {
            _i2c.write(0x0e, control);
	}
}

void DS3231::dump(void)
{
    uint8_t data[19];
    _i2c.read(0, sizeof(data), data);
    Serial.println();
    Serial.println("DS3231 register contents");
    for (uint8_t ind=0; ind<sizeof(data); ind++) {
        Serial.printlnf("%02x: %02x", ind, data[ind]);
    }
}


int DS3231::enableAlarm(uint8_t alarmId)
{
    if (alarmId > DS_ALARM_MAX) return -1;

    _alarm[alarmId].enable();

    return 0;
}


int DS3231::disableAlarm(uint8_t alarmId)
{
    if (alarmId == 0xFF) {
	// clear all alarms
	for (alarmId=0; alarmId <= DS_ALARM_MAX; alarmId++) {
	    _alarm[alarmId].disable();
	}
    } else if (alarmId > DS_ALARM_MAX) {
	return -1;
    } else {
        _alarm[alarmId].disable();
    }

    return 0;
}

bool DS3231::alarmTriggered(uint8_t alarmId)
{
    if (alarmId > DS_ALARM_MAX) return -1;

    return _alarm[alarmId].triggered();
}

int DS3231::clearAlarm(uint8_t alarmId)
{
    if (alarmId == 0xFF) {
	// clear all alarms
	for (alarmId=0; alarmId <= DS_ALARM_MAX; alarmId++) {
	    _alarm[alarmId].clear();
	}
    } else if (alarmId > DS_ALARM_MAX) {
	return -1;
    } else {
        _alarm[alarmId].clear();
    }

    return 0;
}

int DS3231::setAlarm(uint8_t alarmId, int8_t second, int8_t minute, int8_t hour, int8_t dayOfWeek, int8_t day)
{
    if (alarmId > DS_ALARM_MAX) return -1;

    _alarm[alarmId].set(second, minute, hour, dayOfWeek, day);

    return 0;
}

int DS3231::setCountdown(uint8_t alarmId, uint32_t seconds)
{
    if (alarmId > DS_ALARM_MAX) return -1;

    DateTime dt = now() + TimeSpan(seconds);

    int res = setAlarm(alarmId, dt.second(), dt.minute(), dt.hour());

    if (res >= 0) {
        res = enableAlarm(alarmId);
    }

    return res;
}

void DS3231::setFrequency(uint8_t frequency)
{
    _i2c.modify(DS_REG_CONTROL, DS_CONTROL_FREQ_MASK, frequency << DS_CONTROL_FREQ_OFFSET);

}

void DS3231::enableSquareWave(uint8_t frequency, bool batteryBacked)
{
    setFrequency(frequency);
    _i2c.modify(DS_REG_CONTROL,
           DS_CONTROL_INTCN | DS_CONTROL_BBSQW,
           DS_CONTROL_INTCN | (batteryBacked ? DS_CONTROL_BBSQW : 0));
}

/**
 * Verify comms to the DS3231.
 */
bool DS3231::exists(void)
{
    // TBD
    return true;
}
