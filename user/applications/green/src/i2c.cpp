/*
 * i2c.cpp
 *
 * Copyright (c) 2020 Darran Hunt
 * All rights reserved.
 */

#include "i2c.h"

#if 0
I2C::I2C(uint8_t address)
{       
    _addr = address;
}
#endif

uint8_t I2C::read(uint8_t reg) 
{
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(_addr, (uint8_t)1);
    return Wire.read();	
}

void I2C::read(uint8_t reg, uint8_t count, uint8_t *data)
{
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(_addr, (uint8_t)count);

    while (count--) {
        *data++ = Wire.read();
    }
}

void I2C::write(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

void I2C::write(uint8_t reg, uint8_t count, const uint8_t *value) 
{
    Wire.beginTransmission(_addr);
    Wire.write(reg);

    while (count--) {
        Wire.write(*value++);
    }
    Wire.endTransmission();
}

uint8_t I2C::modify(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t currentValue = read(reg);

    write(reg, (currentValue & ~mask) | (value & mask));

    uint8_t newValue = read(reg);
    Serial.printlnf("I2C::modify(0x%02x, 0x%02x, 0x%02x) 0x%02x -> 0x%02x",
            reg, mask, value, currentValue, newValue);

    return currentValue;
}
