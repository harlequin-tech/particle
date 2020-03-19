/*
 * pcf8523_regs.h
 *
 */

#ifndef PCF8523_REGS_H_
#define PCF8523_REGS_H_

#define PCF_ALARM_1      0
#define PCF_ALARM_MAX    0

#define PCF_I2C_ADDR                  0x68

#define PCF_REG_CONTROL_1             0x00
#define PCF_REG_CONTROL_2             0x01
#define PCF_REG_CONTROL_3             0x02
#define PCF_REG_SECONDS               0x03
#define PCF_REG_MINUTES               0x04
#define PCF_REG_HOURS                 0x05
#define PCF_REG_DAYS                  0x06
#define PCF_REG_WEEKDAYS              0x07
#define PCF_REG_MONTHS                0x08
#define PCF_REG_YEARS                 0x09
#define PCF_REG_MINUTE_ALARM          0x0A
#define PCF_REG_HOUR_ALARM            0x0B
#define PCF_REG_DAY_ALARM             0x0C
#define PCF_REG_WEEKDAY_ALARM         0x0D
#define PCF_REG_OFFSET                0x0E
#define PCF_REG_TMR_CLKOUT_CTRL       0x0F
#define PCF_REG_TMR_A_FREQ_CTRL       0x10
#define PCF_REG_TMR_A_REG             0x11
#define PCF_REG_TMR_B_FREQ_CTRL       0x12
#define PCF_REG_TMR_B_REG             0x13

#define PCF_CONTROL_1_CAP_SEL_BIT (1<<7)
#define PCF_CONTROL_1_T_BIT       (1<<6)
#define PCF_CONTROL_1_STOP_BIT    (1<<5)
#define PCF_CONTROL_1_SR_BIT      (1<<4)
#define PCF_CONTROL_1_1224_BIT    (1<<3)
#define PCF_CONTROL_1_SIE_BIT     (1<<2)
#define PCF_CONTROL_1_AIE_BIT     (1<<1)
#define PCF_CONTROL_1CIE_BIT      (1<<0)

#define PCF_CONTROL_2_WTAF_BIT    (1<<7)
#define PCF_CONTROL_2_CTAF_BIT    (1<<6)
#define PCF_CONTROL_2_CTBF_BIT    (1<<5)
#define PCF_CONTROL_2_SF_BIT      (1<<4)
#define PCF_CONTROL_2_AF_BIT      (1<<3)
#define PCF_CONTROL_2_WTAIE_BIT   (1<<2)
#define PCF_CONTROL_2_CTAIE_BIT   (1<<1)
#define PCF_CONTROL_2_CTBIE_BIT   (1<<0)

#define PCF_CLKOUT_COF            (0x07 << 3)

#define PCF_SECONDS_OS_BIT        7
#define PCF_SECONDS_10_BIT        6
#define PCF_SECONDS_10_LENGTH     3
#define PCF_SECONDS_1_BIT         3
#define PCF_SECONDS_1_LENGTH      4

#define PCF_ALARM_DISABLE         0x80

#define PCF_MINUTES_10_BIT        6
#define PCF_MINUTES_10_LENGTH     3
#define PCF_MINUTES_1_BIT         3
#define PCF_MINUTES_1_LENGTH      4

#define PCF_HOURS_MODE_BIT        3 // 0 = 24-hour mode, 1 = 12-hour mode
#define PCF_HOURS_AMPM_BIT        5 // 2nd HOURS_10 bit if in 24-hour mode
#define PCF_HOURS_10_BIT          4
#define PCF_HOURS_1_BIT           3
#define PCF_HOURS_1_LENGTH        4

#define PCF_WEEKDAYS_BIT          2
#define PCF_WEEKDAYS_LENGTH       3

#define PCF_DAYS_10_BIT           5
#define PCF_DAYS_10_LENGTH        2
#define PCF_DAYS_1_BIT            3
#define PCF_DAYS_1_LENGTH         4

#define PCF_MONTH_10_BIT          4
#define PCF_MONTH_1_BIT           3
#define PCF_MONTH_1_LENGTH        4

#define PCF_YEAR_10H_BIT          7
#define PCF_YEAR_10H_LENGTH       4
#define PCF_YEAR_1H_BIT           3
#define PCF_YEAR_1H_LENGTH        4

#endif
