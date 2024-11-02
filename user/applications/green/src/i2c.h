/*
 * i2c.h
 *
 */

#ifndef I2C_H_
#define I2C_H_

#include <Arduino.h>
#include <Wire.h>

//#define Serial Serial1

/**
 * I2C
 */
class I2C {
    public:
        I2C() {};
        uint8_t read(uint8_t reg);
        void read(uint8_t reg, uint8_t count, uint8_t *data);
        void write(uint8_t reg, uint8_t value);
        void write(uint8_t reg, uint8_t count, const uint8_t *data);
        uint8_t modify(uint8_t reg, uint8_t value, uint8_t mask=0xFF);
        void setAddr(uint8_t addr) { _addr = addr; }
        uint8_t getAddr(void) { return _addr; }

    private:
        uint8_t _addr;  // I2C address
};
#endif
