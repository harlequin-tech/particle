/**
*
* green.cpp
*
* green sensor monitoring apps
*
*/

#include "Particle.h"

//SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_MODE(MANUAL);
STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

#define CONFIG_MANUAL_CONNECT
#define CONFIG_RTC
#undef CONFIG_SLEEP
#define CONFIG_WATER

#define LED_PERIOD 10000

const uint16_t ledPin = D7;
const uint16_t wakePin = D8;

#include "Wire.h"

#include "sleeper.h"
Sleeper sleeper(RTC_PCF8532, PCF_I2C_ADDR, wakePin, ledPin);

#ifdef CONFIG_WATER
#include "range.h"
Range water;
const uint16_t waterEchoPin = D5;
const uint16_t waterInterruptPin = D5;
const uint16_t waterTriggerPin = D6;
#endif

#include "pub.h"

Pub publisher;

#define BATT_R1                2000.0
#define BATT_R2                806.0
#define BATT_ADC_UNIT        0.0008                // 3.3V/4096 = 0.8mV per unit
#define BATT_R_ADJ        1.403                // resistor scale
#define VBATT(batt_adc)        (batt_adc * ((BATT_R1 + BATT_R2)/BATT_R2) * 0.0008)

//retained struct __attribute__((__packed__)) {
retained struct {
    uint32_t wakeCount;
} retainedConfig;

LEDStatus ledStatus;
volatile bool woken = false;

char testBuf[622];

double vbatt = 0;
double waterDepth = 0;

uint32_t connectStart = 0;

static void publishBatteryVoltage(void)
{
    uint16_t vbatt_adc = analogRead(BATT);
    vbatt = vbatt_adc * (BATT_ADC_UNIT * BATT_R_ADJ);
    Particle.publish("Batt_Voltage", String::format("%.2f", vbatt), NO_ACK);
    Serial.print("VBATT ADC:   "); Serial.println(vbatt_adc);
    Serial.printlnf("Battery %.2f V\n", vbatt);
}

bool firstRun=true;

void setup() 
{
    int wakeReason = System.wakeUpReason();
    firstRun = (wakeReason != WAKEUP_REASON_PIN_OR_RTC) &&
               (wakeReason != WAKEUP_REASON_PIN);

    char buf[64];

#ifdef CONFIG_CONNECT
    Mesh.on();
    Mesh.connect();
#endif

    pinMode(ledPin, OUTPUT);
    pinMode(BATT, INPUT);
    ledStatus.off();

    retainedConfig.wakeCount++;

    //Time.zone(1);
    //Time.beginDST();

    Wire.begin();
    Wire.setClock(400000);

    Serial.begin(115200);

#ifdef CONFIG_SPI
    SPI.begin(SPI_MODE_MASTER, D14);
    SPI.setClockSpeed(5, MHZ);
    digitalWrite(D14, HIGH);
#endif

    delay(5000);

    while (1) {
        Serial.println("Awake");
        delay(5000);
    }

    Serial.println("Testing RTC");

    DateTime dt(2020, 1, 25, 13, 18, 0);

    sleeper.enableRTC();

    Serial.println("Set time");
    sleeper.rtc.set(dt);

    sleeper.rtc.dump();

    Serial.println("Get time");
    dt = sleeper.rtc.now();
    Serial.printlnf(dt.str(buf, sizeof(buf)));

#if 0
    delay(5000);
    dt = sleeper.rtc.now();
    Serial.println("Get time after 5 seconds");
    Serial.printlnf(dt.str(buf, sizeof(buf)));

    Serial.println();

    sleeper.rtc.dump();

    Serial.println("Testing alarm interrupt (10 seconds)");
    Serial.printlnf("%5d D8=%u", millis(), digitalRead(D8));

    sleeper.rtc.setCountdown(0, 10);
    sleeper.rtc.dump();

    uint32_t last = millis();
    uint32_t count=0;
    uint8_t lastd8 = digitalRead(D8);
    Serial.printlnf("%5d D8=%u (countdown enabled)", last, lastd8);
    while (1) {
        uint32_t now = millis();
        count++;
        uint8_t rtcOut = digitalRead(D8);
        if (rtcOut != lastd8) {
            Serial.printlnf("%5d D8 changed to %u", now, rtcOut);
            lastd8 = rtcOut;
        }
        if ((now - last) >= 5000) {
            Serial.printlnf("%5d D8=%u", now, digitalRead(D8));
            last = now;
            if (count++ % 2 == 0) {
                sleeper.rtc.dump();
            }
        }
    }
#endif

    if (firstRun) {
        memset(&retainedConfig, 0, sizeof(retainedConfig));
        retainedConfig.wakeCount = 0;
        delay(2000);
        Serial.println("Running startup mode ...");
        Serial.printlnf("WakeCount: %u",retainedConfig.wakeCount);
    }

    Particle.variable("vbatt", vbatt);
    Particle.variable("depth", waterDepth);

    if (firstRun) {
        delay(2000);
        sleeper.enableRTC();
    }

    if (!firstRun) {
        delay(500);
        Serial.println("Checking for shock data");
        Serial.println();
    }
#ifdef CONFIG_WATER
    water.begin(waterTriggerPin, waterEchoPin, waterEchoPin);
#endif

    Serial.printlnf("WakeCount: %u\n", retainedConfig.wakeCount);
    publisher.begin("tactical_shock", 2042, "192.168.1.61", 2042);

    sleeper.enable();
}

void mainLoop(uint32_t now)
{
    static bool ledOn = true;
    static uint32_t ledTick = 0;
    static uint32_t lastTick = 0;

    if (ledOn && ((now - ledTick) > 100)) {
        ledOn = false;
        digitalWrite(ledPin, LOW);
    }

    if ((now - lastTick) >= LED_PERIOD) {
        sleeper.printTimestamp();
        ledOn = true;
        digitalWrite(ledPin, HIGH);
        ledTick = now;
        lastTick = now;
    }
}

void testPublish(void)
{
    uint16_t ind;
    for (ind=0; ind<sizeof(testBuf); ) {
        ind += snprintf(&testBuf[ind], sizeof(testBuf) - ind, "%u ", ind);
    }
    Serial.printlnf("Publishing test with %u bytes", ind);
    Particle.publish("test", testBuf, PRIVATE);
}

typedef enum {
    CS_START=0,
    CS_MESH_CONNECT,
    CS_PARTICLE_CONNECT,
    CS_TIME_SYNC,
    CS_PUBLISH_WATER,
    CS_CONNECTED,
    CS_CONNECTED_IDLE,
    CS_CONNECTED_WATER,
    CS_CONNECTED_SLEEP,
    CS_CONNECTED_RTC_SLEEP,
} connectState_e;


void loop() 
{
    static uint32_t connectState = CS_START;
    static uint32_t sleepGuard = 0;
    static uint32_t report = 0;

    char buf[128];
    uint32_t now = millis();

    switch (connectState) {
    case CS_START:
        Serial.printlnf("Connecting %u ms", now);
        connectStart = now;
        Mesh.connect();
        connectState = CS_MESH_CONNECT;
        Serial.println("State -> CS_MESH_CONNECT");
        return;

    case CS_MESH_CONNECT:
        if (!Mesh.ready()) return;
        Serial.printlnf("Mesh connected after %u ms", now - connectStart);
        connectStart = now;
        Particle.connect();
        connectState = CS_PARTICLE_CONNECT;
        Serial.println("State -> CS_PARTICLE_CONNECT");
        return;

    case CS_PARTICLE_CONNECT:
        Particle.process();
        water.loop(now);
        if ((now - connectStart) >= 5000) {
            WITH_LOCK(Serial) {
                Serial.printlnf("Particle not connected after %u ms", now - connectStart);
                Serial.printlnf("---- pings: %u  echos: %u, ints: %u high:%u low:%u", 
                        water.getPingCount(), 
                        water.getEchoCount(),
                        water.getInterruptCount(),
                        water.getInterruptHigh(),
                        water.getInterruptLow());
            }
            connectStart = now;
            Particle.connect();
        }
        if (!Particle.connected()) return;
        Serial.printlnf("Particle connected after %u ms", now - connectStart);
        connectStart = now;
        connectState = CS_TIME_SYNC;
        Serial.println("State -> CS_TIME_SYNC");
        return;

    case CS_TIME_SYNC: {
        Particle.process();
        if (!Time.isValid()) return;
        connectState = CS_CONNECTED;
        Serial.println("State -> CS_CONNECTED");

        DateTime wakeTime = DateTime((uint32_t)Time.now) - TimeSpan(now / 1000);

        Serial.println(Time.timeStr());
        Serial.printlnf(": Time valid after %u ms", now - connectStart);
        Serial.print("Woke at ");
        sleeper.printTimestamp(wakeTime);
        Serial.println();
        return;
        }

#ifdef CONFIG_WATER
    case CS_PUBLISH_WATER: {
        char tbuf[64];
        Particle.process();
        water.loop(now);
        if ((now - report) > 30000) {
            report = now;
            DateTime rtcNow = sleeper.rtc.now();
            if (water.idle()) {
                Serial.printlnf("%s: (1) water sensor is idle. Ping sent count %u",
                        rtcNow.str(buf, sizeof(buf)), water.getPingSentCount());
            } else {
                Serial.printlnf("%s: (1) water sensor is active. Ping sent count %u",
                        rtcNow.str(buf, sizeof(buf)), water.getPingSentCount());
            }

        }
        if (!water.newReading()) {
            return;
        }
        waterDepth = water.getRange();
        Particle.publish("depth", String::format("%.1f", waterDepth), NO_ACK);

        snprintf(buf, sizeof(buf), "\"timestamp\":\"%s\",\"sensorId\":%u,\"sensorType\":\"double\",\"depth\":%.2f",
                sleeper.rtc.now().str(tbuf, sizeof tbuf, true),
                1,
                waterDepth);
        Mesh.publish("sense", buf);
        Serial.printlnf("water depth: %.1f", waterDepth);
        connectState = CS_CONNECTED_IDLE;
        Serial.println("State -> CS_CONNECTED_IDLE");
        sleepGuard = now;
        break;
        }
#endif

    case CS_CONNECTED_SLEEP:
        if ((now - sleepGuard) >= 5000) {
            Serial.printlnf("sleeping at %u...", now);
            Serial.printf("Sleeping. WakeCount %u\n", retainedConfig.wakeCount);
            digitalWrite(ledPin, LOW);
            connectState = CS_START;
            Serial.println("State -> CS_START");
            System.sleep(wakePin, FALLING, 10);
            digitalWrite(ledPin, HIGH);
            Serial.printlnf("woke up at %u", millis());
        }
        break;

    case CS_CONNECTED: {
        Particle.process();
        publishBatteryVoltage();

#ifdef CONFIG_WATER
        Serial.println("Running water ping");
        water.ping(5);
        connectState = CS_PUBLISH_WATER;
        Serial.println("State -> CS_PUBLISH_WATER");
#else
        connectState = CS_CONNECTED_IDLE;
        Serial.println("State -> CS_CONNECTED_IDLE");
#endif
        }
        break;

    case CS_CONNECTED_IDLE:
	break;

    default:
        break;
    }

    water.loop();
    Particle.process();

    if (firstRun && Time.isValid()) {
        uint32_t connectTime = now - connectStart;
        DateTime rtcNow = sleeper.rtc.now();
        uint32_t meshEpoch = Time.now();

        if ((meshEpoch - rtcNow.unixtime()) > 1) {
            // Update RTC to match mesh time
            DateTime dt = DateTime(meshEpoch);
            sleeper.rtc.set(dt);
            Serial.println("Synced RTC to mesh time");
            dt = sleeper.rtc.now();
            Serial.printlnf(dt.str(buf, sizeof(buf)));
        }

        // Connection delay
        snprintf(buf, sizeof(buf), "%.3f", connectTime/1000.0);
        Particle.publish("Connect", buf, PRIVATE);

        Serial.print("- mesh time: ");
        Serial.print(Time.timeStr());
        Serial.print(" - connect took ");
        Serial.print(connectTime);
        Serial.println(" ms");
        firstRun = false;
    }

    sleeper.loop(now);

#ifdef CONFIG_WATER
    water.loop(now);
#endif

    mainLoop(now);

    if (woken) {
        Serial.println("Woke up\n\n");
        woken = false;
        sleeper.clear();
    }

    if ((now - report) > 30000) {
        report = now;
        DateTime rtcNow = sleeper.rtc.now();
        if (water.idle()) {
            Serial.printlnf("%s: water sensor is idle. Ping count %u",
                    rtcNow.str(buf, sizeof(buf)), water.getPingCount());
        } else {
            Serial.printlnf("%s: water sensor is active. Ping count %u",
                    rtcNow.str(buf, sizeof(buf)), water.getPingCount());
        }

    }
}
