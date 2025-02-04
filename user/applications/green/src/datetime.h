#ifndef DATETIME_H_
#define DATETIME_H_

/*
 * datetime.h
 *
 */

#include <Arduino.h>

#define SECONDS_FROM_1970_TO_2000 946684800

// Timespan which can represent changes in time with seconds accuracy.
class TimeSpan {
public:
    TimeSpan(int32_t seconds = 0);
    TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds);
    TimeSpan(const TimeSpan& copy);
    int16_t days() const         { return _seconds / 86400L; }
    int8_t  hours() const        { return _seconds / 3600 % 24; }
    int8_t  minutes() const      { return _seconds / 60 % 60; }
    int8_t  seconds() const      { return _seconds % 60; }
    int32_t totalseconds() const { return _seconds; }

    TimeSpan operator+(const TimeSpan& right);
    TimeSpan operator-(const TimeSpan& right);

protected:
    int32_t _seconds;
};

typedef enum {
        DT_FORMAT_DEFAULT=0,
        DT_FORMAT_ISO8601=1
} dt_format_t;

// DateTime (get everything at once) from JeeLabs / Adafruit
// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
public:
    DateTime(uint32_t t =0);
    DateTime(uint16_t year,
            uint8_t month,
            uint8_t day,
            uint8_t hour=0,
            uint8_t min=0,
            uint8_t sec=0);
    DateTime (const char* date, const char* time);
    uint16_t year() const       { return 2000 + yOff; }
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }
    uint8_t dayOfTheWeek() const;

    char *str(char *buf, int len, uint8_t format=DT_FORMAT_DEFAULT, uint32_t micros=0);

    // 32-bit times as seconds since 1/1/2000
    long secondstime() const;
    // 32-bit times as seconds since 1/1/1970
    // THE ABOVE COMMENT IS CORRECT FOR LOCAL TIME; TO USE THIS COMMAND TO
    // OBTAIN TRUE UNIX TIME SINCE EPOCH, YOU MUST CALL THIS COMMAND AFTER
    // SETTING YOUR CLOCK TO UTC
    uint32_t unixtime(void) const;

    DateTime operator+(const TimeSpan& span);
    DateTime operator-(const TimeSpan& span);
    TimeSpan operator-(const DateTime& right);

protected:
    uint8_t yOff, m, d, hh, mm, ss;
    int32_t secondsOffset;        // DST or timezone offset
};


/*
 * Low level definitions
 */
typedef union  __attribute__((__packed__)) {
    uint8_t data[5];
    struct __attribute__((__packed__)) {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;            // day of week for PCF, day for DS
        uint8_t dayAlt;         // day of week for DS, day for PCF
    } setting;
} alarmRaw_t;

typedef union  __attribute__((__packed__)) {
    uint8_t data[7];
    struct __attribute__((__packed__)) {
        uint8_t second;
        uint8_t minute;
        uint8_t hour;
        uint8_t day;            // day of week for PCF, day for DS
        uint8_t dayAlt;         // day of week for DS, day for PCF
        uint8_t month;
        uint8_t year;
    } value;
} dateRaw_t;

#endif
