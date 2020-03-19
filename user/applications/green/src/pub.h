#ifndef PUB_H_
#define PUB_H_

#include <arpa/inet.h>

class Pub {
    public:
        void begin(const char *deviceName, uint16_t localPort, const char *remoteAddr, uint16_t remotePort);
        void stop(void);
        void setDest(uint32_t remoteIP, uint16_t remotePort);
        void publish(const char *channel, const char *value);

    private:
        UDP _udp;
        uint16_t _localPort;
        uint16_t _remotePort;
        in_addr _remoteIP;
        const char *_deviceName;
};
#endif
