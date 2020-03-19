#ifndef RANGE_H_
#define RANGE_H_

#define RANGE_SAMPLE_MAX 4

class Range {
    public:
        void begin(const uint16_t triggerPin, const uint16_t echoPin, const uint16_t interruptPin);
        void publish(const char *channel, const char *value);
        float loop(uint32_t now=0);
        float getRange(void);
        int getRange(float *range);
        void trigger(void);
        void ping(uint32_t count=1, bool wait=false);
        bool newReading(void) { return _newReading; };
        void echoHandler(void);
        uint32_t getEchoCount(void) { return _echoCount; };
        uint32_t getPingCount(void) { return _pingCount; };
        uint32_t getPingSentCount(void) { return _pingSentCount; };
        uint32_t getInterruptCount(void) { return _interruptCount; };
        uint32_t getInterruptHigh(void) { return _interruptHigh; };
        uint32_t getInterruptLow(void) { return _interruptLow; };

        void setTemperture(float temperature) { _temperature = temperature; }
        bool idle(void) { return !(_timing || _pinging); }

    private:
        uint16_t _triggerPin;
        uint16_t _echoPin;
        uint16_t _interruptPin;

        volatile uint32_t _pulseStart;
        volatile uint32_t _pulseDuration;
        float _temperature = 20;
        volatile bool _pinging = false;
        volatile bool _newPulse = false;
        volatile bool _timing = false;
        volatile bool _newReading = false;

        uint32_t _pingStart;            // timeout guard for lack of response
        bool _pingTimeout;
        int _pingSendCount = 0;
        int _pingSentCount = 0;
        uint32_t _pingCount = 0;
        uint32_t _echoCount = 0;
        volatile uint32_t _interruptCount = 0;
        volatile uint32_t _interruptHigh = 0;
        volatile uint32_t _interruptLow = 0;

        bool _sendPing = false;
        uint32_t _pingGuard = 0;

        uint32_t _last = 0;
        float _range = 999;
        float _rangeSum = 0;
};

#endif
