#include <Arduino.h>
#include "range.h"

#define PING_US_PER_CM	58.0

#define PIN_RPM		(1<<PCINT5)	// PB5 - D9
#define PIN_ECHO	(1<<PCINT4)	// PB4 - D8

#define PIN_MASK (PIN_ECHO | PIN_RPM)


/*
 * Algorithm
 *
 * ping(count)
    _rangeSum = 0;
    _pingSendCount = count;
    trigger();
        _timing = false;
        _pinging = true;
        _pingCount++;
    _pingSentCount = 1;



 */

/**
 * interrupt handler called when the range finder's echo pin
 * changes state.  Measures the period of time that the pin is
 * held high, which represents the distance.
 */
void Range::echoHandler(void)
{
    bool high = digitalRead(_echoPin);
    _interruptCount++;

    if (high) {
        _interruptHigh++;
    } else {
        _interruptLow++;
    }

    if (_pinging) {
	if (high) {
	    _pulseStart = micros();
	    _pinging = false;
	    _timing = true;
	}
    } else if (_timing) {
	if (!high) {
	    _pulseDuration = micros() - _pulseStart;
	    _timing = false;
	    _newPulse = true;
	}
    }
}

void Range::begin(const uint16_t triggerPin, const uint16_t echoPin, const uint16_t interruptPin)
{
    _last = 0;
    _range = 999;

    _pinging = false;
    _newPulse = false;
    _timing = false;
    _echoCount = 0;

    _triggerPin = triggerPin;
    _echoPin = echoPin;
    _interruptPin = interruptPin ;

    pinMode(_triggerPin, OUTPUT);
    digitalWrite(_triggerPin, LOW);

    // pull up echo pin
    pinMode(_echoPin, INPUT);
    digitalWrite(_echoPin, HIGH);

    attachInterrupt(_interruptPin, &Range::echoHandler, this, CHANGE);

}

void Range::trigger(void)
{
    _pingStart = millis();
    _pingTimeout = false;

    digitalWrite(_triggerPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(_triggerPin, LOW);
    delayMicroseconds(10);
    digitalWrite(_triggerPin, HIGH);
    _timing = false;
    _pinging = true;
    _pingCount++;
}

void Range::ping(uint32_t count, bool wait)
{
    _rangeSum = 0;
    _pingSendCount = count;
    trigger();
    _pingSentCount = 1;
}

int Range::getRange(float *range)
{
    *range = _range;
    if (newReading()) {
        _newReading = false;
        if (!_pingTimeout) {
            return 1;
        } else {
            return -1;
        }
    } else {
        return 0;
    }
}

float Range::getRange()
{
    _newReading = false;
    return _range;
}

/**
 * main range loop.  Checks for new pulse measurements
 * and records the range.
 * Initiates new pings if the send count is not yet zero.
 */
float Range::loop(uint32_t now) 
{
    if (now == 0) {
        now = millis();
    }

    if (_pinging && ((now - _pingStart) > 200)) {
        Serial.printlnf("Range: ping timeout");
        // timeout ping, no response
        _pinging = false;
        _newReading = true;
        _pingTimeout = true;
        return _range;
    }

    if (_newPulse) {
        /*
         * speed of sound c = 331.3 + 0.606 * Temp
         */
        float speedOfSound = 20000.0 / (331.3 + 0.606 * _temperature);

        float pulseRange = _pulseDuration / speedOfSound;
	//float range = (_pulseDuration / PING_US_PER_CM);
	_rangeSum += pulseRange;
	_newPulse = false;
        _echoCount++;
	_last = millis();
        Serial.printlnf("range[%u]: %0.3f", _pingSentCount-1, pulseRange);
        if (_pingSentCount < _pingSendCount) {
            _pingGuard = now;
            _sendPing = true;
        } else {
            _newReading = true;
            _range = _rangeSum / _pingSentCount;
            _rangeSum = 0;
            Serial.printlnf("Average range: %0.3f", _range);
        }
    } else if (_sendPing && ((now - _pingGuard) >= 200)) {
        trigger();
        _pingSentCount++;
        _sendPing = false;
    }

    return _range;
}
