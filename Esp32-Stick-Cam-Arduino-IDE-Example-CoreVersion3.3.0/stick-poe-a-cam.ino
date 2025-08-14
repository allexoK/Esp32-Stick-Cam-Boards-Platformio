/*
 * ESP32 + W5500 (EthernetESP32) + WebServer + ESP32-CAM (JPEG snapshot)
 * - Uses new EthernetESP32 driver API: W5500Driver + WebServer + mDNS
 * - Serves a low-res JPEG at /cam-lo.jpg and a simple index page at /
 */

#include <Arduino.h>
#include <SPI.h>
#include <EthernetESP32.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp32cam.h>

#if !defined(ESP32)
#  error "This code is for ESP32."
#endif

// --------- W5500 SPI pins (change to match your board) ---------
// Use output-capable pins for CS; INT must be an input-capable pin.
#define W5500_SCK   21
#define W5500_MISO  14
#define W5500_MOSI  1
#define W5500_CS    39    // DO NOT use 34–39 (input-only) for CS
#define W5500_INT   38

// Create the Ethernet driver for W5500
W5500Driver driver(W5500_CS, W5500_INT);

// Standard Arduino WebServer (works over Ethernet)
WebServer server(80);

// ---------- (Optional) mDNS hostname ----------
const char* mdnsHost = "esp32";

// ---------- Camera pin map (use your actual wiring) ----------
namespace my_pins {
  constexpr esp32cam::Pins MyESP32CAM{
    D0: 11, D1: 9,  D2: 8,  D3: 10,
    D4: 12, D5: 18, D6: 17, D7: 16,
    XCLK: 15, PCLK: 13, VSYNC: 6, HREF: 7,
    SDA: 4,  SCL: 5,  RESET: -1, PWDN: -1,
  };
}

// ---------- JPEG serving helpers ----------
static void serveJpg()
{
  uint32_t t0 = millis();
  auto frame = esp32cam::capture();
  uint32_t t1 = millis();

  if (!frame) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "text/plain", "capture fail");
    return;
  }

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");

  auto client = server.client();         // transport-agnostic
  uint32_t t2 = millis();
  frame->writeTo(client);
  uint32_t t3 = millis();

  Serial.printf("capt %lu ms, send %lu ms, write %lu ms\r\n",
                t1 - t0, t2 - t1, t3 - t2);
}

static void handleJpgLo()
{
  Serial.println("cam request");
  if (!esp32cam::Camera.changeResolution(esp32cam::Resolution::find(320, 240))) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

// ---------- Basic pages ----------
static void handleRoot()
{
  server.send(
    200, "text/html",
    "<!doctype html><html><body>"
    "<h3>ESP32 Ethernet + CAM</h3>"
    "<p><a href='/cam-lo.jpg'>Snapshot (320x240)</a></p>"
    "<img src='/cam-lo.jpg' alt='snapshot'>"
    "</body></html>"
  );
}

static void handleNotFound()
{
  String message = "Not Found\n\nURI: " + server.uri() +
                   "\nMethod: " + String((server.method()==HTTP_GET)?"GET":"POST") +
                   "\nArguments: " + String(server.args()) + "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  // ---- Camera init ----
  esp32cam::Config cfg;
  cfg.setPins(my_pins::MyESP32CAM);
  cfg.setResolution(esp32cam::Resolution::find(320, 240));  // default lo-res
  cfg.setBufferCount(2);
  cfg.setJpeg(80);
  bool camOK = esp32cam::Camera.begin(cfg);
  delay(500);
  Serial.println(camOK ? "CAMERA OK" : "CAMERA FAIL");

  // ---- SPI + Ethernet init (new driver) ----
  SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI); // SCK, MISO, MOSI
  Ethernet.init(driver);

  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin()) {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true) { delay(1); }
  }

  // ---- Optional: mDNS ----
  if (MDNS.begin(mdnsHost)) {
    Serial.printf("MDNS responder started: http://%s.local/\r\n", mdnsHost);
  }

  // ---- HTTP routes ----
  server.on("/", handleRoot);
  server.on("/cam-lo.jpg", handleJpgLo);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();
  delay(2);  // allow CPU to switch to other tasks
}
