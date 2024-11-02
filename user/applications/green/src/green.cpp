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
const static SerialLogHandler logHandler(115200, LOG_LEVEL_ALL);

#include "Wire.h"
#include "Wire.h"
//#include "Oled.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#define TFT_CS         14    // N/C
#define TFT_RST        4     // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         3

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define CONFIG_WATER
#undef TEST_RTC_SLEEP

#define LED_PERIOD 10000

const uint16_t ledPin = D7;
const uint16_t wakePin = D8;


#include "sleeper.h"
Sleeper sleeper(RTC_PCF8532, PCF_I2C_ADDR, wakePin, ledPin);

#ifdef CONFIG_WATER
#include "range.h"
Range water;
const uint16_t waterEchoPin = D5;
const uint16_t waterInterruptPin = D5;
const uint16_t waterTriggerPin = D6;
const uint16_t waterPowerPin = D3;
#endif

#include "pub.h"

Pub publisher;

#define BATT_R1           2000.0
#define BATT_R2           806.0
#define BATT_ADC_UNIT     0.0008      // 3.3V/4096 = 0.8mV per unit
#define BATT_R_ADJ        1.403       // resistor scale
#define VBATT(batt_adc)   (batt_adc * ((BATT_R1 + BATT_R2)/BATT_R2) * 0.0008)

//retained struct __attribute__((__packed__)) {
retained struct {
    bool rtcSet;
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
#ifdef USE_PARTICLE_IO
    Particle.publish("Batt_Voltage", String::format("%.2f", vbatt), NO_ACK);
#endif
    Serial.print("VBATT ADC:   "); Serial.println(vbatt_adc);
    Serial.printlnf("Battery %.2f V\n", vbatt);
}

bool firstRun=true;

void blink(int count, uint32_t period)
{
    while (count-- > 0) {
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(period);
    }
}

void testlines(uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(0, 0, x, tft.height()-1, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(0, 0, tft.width()-1, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(tft.width()-1, 0, x, tft.height()-1, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(tft.width()-1, 0, 0, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(0, tft.height()-1, x, 0, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(0, tft.height()-1, tft.width()-1, y, color);
    delay(0);
  }

  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawLine(tft.width()-1, tft.height()-1, x, 0, color);
    delay(0);
  }
  for (int16_t y=0; y < tft.height(); y+=6) {
    tft.drawLine(tft.width()-1, tft.height()-1, 0, y, color);
    delay(0);
  }
}

void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

void testfastlines(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t y=0; y < tft.height(); y+=5) {
    tft.drawFastHLine(0, y, tft.width(), color1);
  }
  for (int16_t x=0; x < tft.width(); x+=5) {
    tft.drawFastVLine(x, 0, tft.height(), color2);
  }
}

void testdrawrects(uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=0; x < tft.width(); x+=6) {
    tft.drawRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color);
  }
}

void testfillrects(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x=tft.width()-1; x > 6; x-=6) {
    tft.fillRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color1);
    tft.drawRect(tft.width()/2 -x/2, tft.height()/2 -x/2 , x, x, color2);
  }
}

void testfillcircles(uint8_t radius, uint16_t color) {
  for (int16_t x=radius; x < tft.width(); x+=radius*2) {
    for (int16_t y=radius; y < tft.height(); y+=radius*2) {
      tft.fillCircle(x, y, radius, color);
    }
  }
}

void testdrawcircles(uint8_t radius, uint16_t color) {
  for (int16_t x=0; x < tft.width()+radius; x+=radius*2) {
    for (int16_t y=0; y < tft.height()+radius; y+=radius*2) {
      tft.drawCircle(x, y, radius, color);
    }
  }
}

void testtriangles() {
  tft.fillScreen(ST77XX_BLACK);
  uint16_t color = 0xF800;
  int t;
  int w = tft.width()/2;
  int x = tft.height()-1;
  int y = 0;
  int z = tft.width();
  for(t = 0 ; t <= 15; t++) {
    tft.drawTriangle(w, y, y, x, z, x, color);
    x-=4;
    y+=4;
    z-=4;
    color+=100;
  }
}

void testroundrects() {
  tft.fillScreen(ST77XX_BLACK);
  uint16_t color = 100;
  int i;
  int t;
  for(t = 0 ; t <= 4; t+=1) {
    int x = 0;
    int y = 0;
    int w = tft.width()-2;
    int h = tft.height()-2;
    for(i = 0 ; i <= 16; i+=1) {
      tft.drawRoundRect(x, y, w, h, 5, color);
      x+=2;
      y+=3;
      w-=4;
      h-=6;
      color+=1100;
    }
    color+=100;
  }
}

void setup() 
{
    int wakeReason = System.wakeUpReason();
    firstRun = (wakeReason != WAKEUP_REASON_PIN_OR_RTC) &&
               (wakeReason != WAKEUP_REASON_PIN);

    char buf[64];

    Serial.println("Testing TFT\n");

    tft.init(240, 240);           // Init ST7789 240x240
    uint16_t time = millis();
    tft.fillScreen(ST77XX_BLACK);
    time = millis() - time;

    Serial.println(time, DEC);
    delay(500);
    testdrawtext((char *)"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", ST77XX_WHITE);
    delay(1000);
    // optimized lines
    testfastlines(ST77XX_RED, ST77XX_BLUE);
    delay(500);

    testdrawrects(ST77XX_GREEN);
    delay(500);

    testfillrects(ST77XX_YELLOW, ST77XX_MAGENTA);
    delay(500);

#ifdef CONFIG_CONNECT
    Mesh.on();
    Mesh.connect();
#endif

    pinMode(ledPin, OUTPUT);
    pinMode(BATT, INPUT);
    ledStatus.off();
    //blink(5, 500);
 
    Wire.begin();
    Wire.setClock(400000);

#ifdef CONFIG_WATER
    water.begin(waterTriggerPin, waterEchoPin, waterEchoPin, waterPowerPin);
#endif

    retainedConfig.wakeCount++;
    Serial.begin(115200);
    Serial.begin(115200);
    waitFor(Serial.isConnected, 10000);
    delay(2000);

    if (firstRun) {
        memset(&retainedConfig, 0, sizeof(retainedConfig));
        retainedConfig.wakeCount = 0;
        retainedConfig.rtcSet = false;
        delay(2000);
        Serial.println("Running startup mode ...");
        Serial.printlnf("WakeCount: %u",retainedConfig.wakeCount);
    }

    //Time.zone(1);
    //Time.beginDST();

    //waitFor(Serial.isConnected, 10000);

#ifdef CONFIG_SPI
    SPI.begin(SPI_MODE_MASTER, D14);
    SPI.setClockSpeed(5, MHZ);
    digitalWrite(D14, HIGH);
#endif


    Serial.println("Green Sense Starting");
    sleeper.init();

    for (int count=0; !sleeper.rtc.ready(); count++) {
        Serial.printlnf("Waiting for RTC... %3u", count);
        delay(1000);
        sleeper.rtc.dump();
    }

    sleeper.rtc.dump();

    Serial.println("Get time");
    DateTime dt = sleeper.rtc.now();
    Serial.printlnf(dt.str(buf, sizeof(buf)));


#ifdef USE_PARTICLE_IO
    Particle.variable("vbatt", vbatt);
    Particle.variable("depth", waterDepth);
#endif

#ifdef TEST_RTC_SLEEP
    Serial.println("Testing RTC sleep and wake");
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
        if ((now - last) >= 10000) {
            Serial.printlnf("%5d D8=%u", now, digitalRead(D8));
            last = now;
            if (count++ % 2 == 0) {
                sleeper.rtc.dump();
            }
        }
    }
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

    Particle.process();

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
#ifdef USE_PARTICLE_IO
        Particle.connect();
        connectState = CS_PARTICLE_CONNECT;
        Serial.println("State -> CS_PARTICLE_CONNECT");
#else
        connectState = CS_CONNECTED;
        Serial.println("State -> CS_CONNECTED");
#endif
        return;

    case CS_PARTICLE_CONNECT:
        water.loop();
        if ((now - connectStart) >= 5000) {
            //WITH_LOCK(Serial) {
                Serial.printlnf("Particle not connected after %u ms", now - connectStart);
                Serial.printlnf("---- pings: %u  echos: %u, ints: %u high:%u low:%u", 
                        water.getPingCount(), 
                        water.getEchoCount(),
                        water.getInterruptCount(),
                        water.getInterruptHigh(),
                        water.getInterruptLow());
            //}
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
        if (!Time.isValid()) return;

        if (!retainedConfig.rtcSet) {
            DateTime dt = DateTime(Time.now());
            sleeper.rtc.set(dt);
            Serial.println("Synced RTC to mesh time");
            dt = sleeper.rtc.now();
            Serial.printlnf(dt.str(buf, sizeof(buf)));
            retainedConfig.rtcSet = true;
        } else {
            Serial.println("RTC already set");
        }

        DateTime wakeTime = DateTime((uint32_t)Time.now()) - TimeSpan(now / 1000);

        Serial.print(Time.timeStr());
        Serial.printlnf(": Time valid after %u ms", now - connectStart);
        Serial.print("Woke at ");
        sleeper.printTimestamp(wakeTime);
        Serial.println();
        Serial.println("State -> CS_CONNECTED");
        connectState = CS_CONNECTED;
        return;
        }

#ifdef CONFIG_WATER
    case CS_PUBLISH_WATER: {
        char tbuf[64];
        water.loop();
        if ((now - report) > 30000) {
            report = now;
            DateTime rtcNow = sleeper.rtc.now();
            if (water.idle()) {
                Serial.printlnf("%s: (1) water sensor is idle. Ping sent count %u",
                        rtcNow.str(buf, sizeof(buf)), water.getPingSentCount());
            } else {
                if (water.poweringUp()) {
                    Serial.printlnf("%s: (1) water sensor is powering up", rtcNow.str(buf, sizeof(buf)));
                } else {
                    Serial.printlnf("%s: (1) water sensor is active. Ping sent count %u",
                            rtcNow.str(buf, sizeof(buf)), water.getPingSentCount());
                }
            }

        }
        if (!water.newReading()) {
            return;
        }
        waterDepth = water.getRange();
#ifdef USE_PARTICLE_IO
        Particle.publish("depth", String::format("%.1f", waterDepth), NO_ACK);
#endif

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


        connectState = CS_CONNECTED;
        Serial.println("State -> CS_CONNECTED");

        DateTime wakeTime = DateTime((uint32_t)Time.now()) - TimeSpan(now / 1000);

        Serial.print(Time.timeStr());
        Serial.printlnf(": Time valid after %u ms", now - connectStart);
        Serial.print("Woke at ");
        sleeper.printTimestamp(wakeTime);
        Serial.println();
        return;
        }

    case CS_CONNECTED: {
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
#ifdef USE_PARTICLE_IO
        Particle.publish("Connect", buf, PRIVATE);
#endif

        Serial.print("- mesh time: ");
        Serial.print(Time.timeStr());
        Serial.print(" - connect took ");
        Serial.print(connectTime);
        Serial.println(" ms");
        firstRun = false;
    }

//    sleeper.loop(now);

#if 0
#ifdef CONFIG_WATER
    water.loop(now);
#endif
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
