#include <Arduino.h>
#include "range.h"


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

void Range::begin(const uint16_t triggerPin, const uint16_t echoPin, const uint16_t interruptPin, const uint16_t powerPin)
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

    _powerPin = powerPin;

    pinMode(_triggerPin, OUTPUT);
    digitalWrite(_triggerPin, LOW);

    // pull up echo pin
    pinMode(_echoPin, INPUT);
    digitalWrite(_echoPin, HIGH);

    if (_powerPin > 0) {
        _powerOn = false;
        pinMode(_powerPin, OUTPUT);
        digitalWrite(_powerPin, LOW);
    }

    attachInterrupt(_interruptPin, &Range::echoHandler, this, CHANGE);

}

int Range::on(void)
{
    if (_powerPin > 0) {
        // only start the power on sequence if not already on
        if (!_powerOn) {
            Serial.println("ping: powering up sensor");
            digitalWrite(_powerPin, HIGH);
            _powerOn = true;
            _poweringUp = true;
            _powerStart = millis();
            return 0;
        } else {
            Serial.println("ping: not powering up sensor - already on");
            return 1;
        }
    } else {
        Serial.println("ping: not powering up sensor - no power pin defined");
        return -1;
    }
}

int Range::off(void)
{
    if (_powerPin > 0) {
        Serial.println("ping: powering down sensor");
        digitalWrite(_powerPin, LOW);
        _powerOn = false;
        _poweringUp = false;
        return 0;
    } else {
        Serial.println("ping: no power pin, not powering down sensor");
        return -1;
    }
}

void Range::trigger(void)
{
    Serial.println("trigger()");
    _pingTimeout = false;

    digitalWrite(_triggerPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(_triggerPin, LOW);
    delayMicroseconds(10);
    _pingStart = millis();
    digitalWrite(_triggerPin, HIGH);
    _timing = false;
    _pinging = true;
    _pingCount++;
}

void Range::ping(uint32_t count, bool wait)
{
    _rangeSum = 0;
    _timing = false;
    _pinging = false;
    _pingSendCount = count;

    if (!_powerReady) {
        on();
        _pingPending = true;
    } else {
        trigger();
        _pingSentCount = 1;
    }
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
float Range::loop(void)
{
    uint32_t now = millis();

    if (_poweringUp) {
        if ((now - _powerStart) < RANGE_POWER_UP_TIME) {
            return _range;
        }
        // powered up
        Serial.println("ping: powered up -> sensor ready");
        _poweringUp = false;
        _powerReady = true;
        if (_pingPending) {
            _pingPending = false;
            _pingSentCount = 1;
            trigger();
            return _range;
        }
    }

    if (_pinging && ((now - _pingStart) > 200)) {
        Serial.printlnf("Range: ping timeout. start %ums - end %ums = %u", _pingStart, now, now - _pingStart);
        // timeout ping, no response
        _pinging = false;
        _newReading = true;
        _pingTimeout = true;
        return _range;
    }

    if (_newPulse) {
        if (_pingSentCount == 1) {
            // discard first sample
            _pingSendCount++;
        } else {
            /*
             * speed of sound c = 331.3 + 0.606 * Temp
             */
            float speedOfSound = 20000.0 / (331.3 + 0.606 * _temperature);

            float pulseRange = _pulseDuration / speedOfSound;
            _rangeSum += pulseRange;
            Serial.printlnf("range[%u]: %0.3f", _pingSentCount-1, pulseRange);
        }
	_newPulse = false;
        _echoCount++;
	_last = millis();
        if (_pingSentCount < _pingSendCount) {
            _pingGuard = now;
            _sendPing = true;
        } else {
            off();
            _newReading = true;
            _range = (_rangeSum / (_pingSentCount-1)) * 10;     // return millimetres
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
