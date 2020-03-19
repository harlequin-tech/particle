#ifndef DS3231_REGS_H_
#define DS3231_REGS_H_

/*
 * ds3231.h
 *
 *
 */

#define DS3231_I2C_ADDR         0x68

#define DS_REG_CURRENT_SECOND   0x00
#define DS_REG_CURRENT_MINUTE   0x01
#define DS_REG_CURRENT_HOUR     0x02
#define DS_REG_CURRENT_WEEKDAY  0x03
#define DS_REG_CURRENT_DAY      0x04
#define DS_REG_CURRENT_MONTH    0x05
#define DS_REG_CURRENT_YEAR     0x06

#define DS_HOUR_12             (1<<6)
#define DS_HOUR_PM             (1<<5)
#define DS_DAY_OF_WEEK         (1<<6)

#define DS_REG_A1_SECOND        0x07
#define DS_REG_A1_MINUTE        0x08
#define DS_REG_A1_HOUR          0x09
#define DS_REG_A1_DAY           0x0a         // bit6 = 1 -> WEEKDAY

#define DS_REG_A2_MINUTE        0x0b
#define DS_REG_A2_HOUR          0x0c         // bit6 = 1 -> 12 hour
#define DS_REG_A2_DAY           0x0d         // bit6 = 1 -> WEEKDAY

#define DS_REG_CONTROL          0x0e
#define DS_REG_STATUS           0x0f
#define DS_REG_AGING_OFFSET     0x10
#define DS_REG_TEMP_MSB         0x11
#define DS_REG_TEMP_LSB         0x12

#define DS_TRIGGER             (1<<7)
#define DS_A1_TRIGGER          (1<<7)

#define DS_A2_TRIGGER          (1<<7)
#define DS_A2_HOUR_PM          (1<<5)      // 1 = PM, 0 = AM
#define DS_A2_HOUR_12          (1<<6)      // 1 = 12 hour, 0 = 24 hour

#define DS_CONTROL_EOSC        (1<<7)      // oscillator 0=enable 1=disable
#define DS_CONTROL_BBSQW       (1<<6)      // battery backed square wave 1=enable, 0=disable
#define DS_CONTROL_CONV        (1<<5)      // convert temperature.  1=do conversion
#define DS_CONTROL_FREQ        (1<<3)      // Square wave frequency select.
#define DS_CONTROL_FREQ_OFFSET 3           // Square wave frequency select.
#define DS_CONTROL_FREQ_MASK   (3<<3)      // 00: 1Hz, 01: 1.024kHz, 10: 4.096kHz, 11: 8.192kHz
#define DS_CONTROL_INTCN       (1<<2)      // interrupt control. 0=Disable (SQW), 1=Enable.
#define DS_CONTROL_A2IE        (1<<1)      // Alarm 2 interrupt enable. 1=enable
#define DS_CONTROL_A1IE        (1<<0)      // Alarm 1 interrupt enable. 1=enable

#define DS_FREQ_1HZ            0x00
#define DS_FREQ_1024HZ         0x01
#define DS_FREQ_4096HZ         0x02
#define DS_FREQ_8192HZ         0x03

#define DS_STATUS_OSF          (1<<7)      // oscillator stop flag. 1: was stopped. Write 0 to clear.
#define DS_STATUS_32KHZ        (1<<3)      // 1: enable 32kHz output. On by default.
#define DS_STATUS_BUSY         (1<<2)      // 1: busy doing TXCO temperature conversion.
#define DS_STATUS_A2           (1<<1)      // 1: Alarm 2 was triggered.  Write to 0 to clear.
#define DS_STATUS_A1           (1<<0)      // 1: Alarm 1 was triggered.  Write to 0 to clear.

#define DS_RATE_1HZ            0x0
#define DS_RATE_1024HZ         0x1
#define DS_RATE_4096HZ         0x2
#define DS_RATE_8192HZ         0x3

#define DS_ALARM_1      0
#define DS_ALARM_2      1
#define DS_ALARM_MAX    1

#define DS_A1_EVERY_SECOND      0x0f       // trigger A1 every second
#define DS_A1_MATCH_SECOND      0x0e       // trigger A1 when second matches
#define DS_A1_MATCH_MINUTE      0x0c       // trigger A1 when minute and second matches
#define DS_A1_MATCH_HOUR        0x08       // trigger A1 when hour, minute, and second matches
#define DS_A1_MATCH_DAY         0x00       // trigger A1 when day, hour, minute, and second matches

#define DS_A2_EVERY_MINUTE      0x07       // trigger A2 every minute
#define DS_A2_MATCH_MINUTE      0x06       // trigger A2 when minute matches
#define DS_A2_MATCH_HOUR        0x04       // trigger A2 when hour and minute matches
#define DS_A2_MATCH_DAY         0x02       // trigger A2 when day, hour, and minute, matches

#define DS_ALARM_SECOND         (1<<0)
#define DS_ALARM_MINUTE         (1<<1)
#define DS_ALARM_HOUR           (1<<2)
#define DS_ALARM_DAY            (1<<3)


#define DS_TRIGGER_SECOND       (1<<0)
#define DS_TRIGGER_MINUTE       (1<<1)
#define DS_TRIGGER_HOUR         (1<<2)
#define DS_TRIGGER_DAY          (1<<3)

#endif
