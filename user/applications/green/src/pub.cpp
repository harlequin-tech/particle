#include <Arduino.h>
#include "pub.h"


void Pub::begin(const char *deviceName, uint16_t localPort, const char *remoteAddr, uint16_t remotePort)
{
    _localPort = localPort;
    _deviceName = deviceName;
    _udp.begin(localPort);

    inet_aton(remoteAddr, &_remoteIP);
    _remotePort = remotePort;
    _remoteIP.s_addr = ntohl(_remoteIP.s_addr);

    Serial.printlnf("pub: %s:%u to 0x%08x %s:%u", _deviceName, _localPort, _remoteIP, remoteAddr, remotePort);
}

void Pub::stop(void)
{
    _udp.stop();
}

void Pub::setDest(uint32_t remoteIP, uint16_t remotePort)
{
    _remoteIP.s_addr = remoteIP;
    _remotePort = remotePort;
}

void Pub::publish(const char *channel, const char *value)
{
    _udp.beginPacket(_remoteIP.s_addr, _remotePort);
    _udp.printf("{ \"device\": \"%s\", \"%s\": %s }", _deviceName, channel, value);
    _udp.endPacket();
    Serial.printlnf("publish({ \"device\": \"%s\", \"channel\":\"%s\", \"value\":%s })", _deviceName, channel, value);
}


