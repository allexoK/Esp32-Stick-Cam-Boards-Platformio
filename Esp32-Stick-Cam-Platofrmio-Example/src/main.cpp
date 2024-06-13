// Debug Level from 0 to 4
#define _ETHERNET_WEBSERVER_LOGLEVEL_       0
#include <Arduino.h>
#include "SPI.h"
#include <esp32cam.h>
// #include <WiFi.h>

#if !( defined(ESP32) )
  #error This code is designed for (ESP32 + W5500) to run on ESP32 platform! Please check your Tools->Board setting.
#endif

#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial0


//////////////////////////////////////////////////////////

// Optional values to override default settings
// Don't change unless you know what you're doing
#define ETH_SPI_HOST        SPI2_HOST
#define SPI_CLOCK_MHZ       25

// Must connect INT to GPIOxx or not working
#define INT_GPIO            38

#define MISO_GPIO           37
#define MOSI_GPIO           35
#define SCK_GPIO            36
#define CS_GPIO             39

// #define INT_GPIO            14

// #define MISO_GPIO           2
// #define MOSI_GPIO           1
// #define SCK_GPIO            3
// #define CS_GPIO             39

//////////////////////////////////////////////////////////

#include <WebServer_ESP32_W5500.h>

WebServer server(80);

// Enter a MAC address and IP address for your controller below.
#define NUMBER_OF_MAC      20

byte mac[][NUMBER_OF_MAC] =
{
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x02 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x03 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x04 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x05 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x06 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x07 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x08 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x09 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0A },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0B },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0C },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0D },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0E },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0F },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x10 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x11 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x12 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x13 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x14 },
};

// Select the IP address according to your local network
IPAddress myIP(192, 168, 2, 232);
IPAddress myGW(192, 168, 2, 1);
IPAddress mySN(255, 255, 255, 0);

// Google DNS Server IP
IPAddress myDNS(8, 8, 8, 8);

int reqCount = 0;                // number of requests received


void serveJpg()
{
  uint32_t t0=millis();
  auto frame = esp32cam::capture();
  uint32_t t1=millis();
  if (frame == nullptr) {
    Serial0.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  uint32_t t2=millis();
  frame->writeTo(client);
  uint32_t t3=millis();
  Serial0.printf("capt %lu,send %lu,write %lu\n\r",t1-t0,t2-t1,t3-t2);

}
 
void handleJpgLo()
{
   Serial0.printf("cam request\n\r");
  if (!esp32cam::Camera.changeResolution(esp32cam::Resolution::find(320, 240))) {
    Serial0.println("SET-LO-RES FAIL");
  }

  serveJpg();
}

namespace my_pins {
    constexpr esp32cam::Pins MyESP32CAM{
      D0: 11,
      D1: 9,
      D2: 8,
      D3: 10,
      D4: 12,
      D5: 18,
      D6: 17,
      D7: 16,
      XCLK: 15,
      PCLK: 13,
      VSYNC: 6,
      HREF: 7,
      SDA: 4,
      SCL: 5,
      RESET: -1,
      PWDN: -1,
    };
} // namespace my_pins

void setup()
{
  Serial0.begin(115200);
  Serial0.println("go\n\r");
  esp32cam::Config cfg;
  cfg.setPins(my_pins::MyESP32CAM);
  cfg.setResolution(esp32cam::Resolution::find(320, 240));
  cfg.setBufferCount(2);
  cfg.setJpeg(80);
  bool ok = esp32cam::Camera.begin(cfg);
  delay(2000);
  Serial0.println(ok ? "CAMERA OK" : "CAMERA FAIL");

  while (!Serial0 && (millis() < 5000));

  Serial0.print(F("\nStart WebServer on "));
  Serial0.print(ARDUINO_BOARD);
  Serial0.print(F(" with "));
  Serial0.println(SHIELD_TYPE);
  Serial0.println(WEBSERVER_ESP32_W5500_VERSION);

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("SPI_HOST:"), ETH_SPI_HOST);
  ET_LOGWARN1(F("MOSI:"), MOSI_GPIO);
  ET_LOGWARN1(F("MISO:"), MISO_GPIO);
  ET_LOGWARN1(F("SCK:"),  SCK_GPIO);
  ET_LOGWARN1(F("CS:"),   CS_GPIO);
  ET_LOGWARN1(F("INT:"),  INT_GPIO);
  ET_LOGWARN1(F("SPI Clock (MHz):"), SPI_CLOCK_MHZ);
  ET_LOGWARN(F("========================="));

  ///////////////////////////////////

  // To be called before ETH.begin()
  ESP32_W5500_onEvent();

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  //bool begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ,
  //           int SPI_HOST, uint8_t *W6100_Mac = W6100_Default_Mac);
  Serial0.printf("%d\n\r",ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST ));
  pinMode(INT_GPIO,INPUT_PULLUP);
  //ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac[millis() % NUMBER_OF_MAC] );

  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
  //ETH.config(myIP, myGW, mySN, myDNS);

  ESP32_W5500_waitForConnect();

  ///////////////////////////////////

  // start the web server on port 80
  server.on("/cam-lo.jpg", handleJpgLo); 
  server.begin();
}

void loop()
{
  server.handleClient();
  delay(5);
}