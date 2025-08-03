#include <Adafruit_Protomatter.h>   // Matrix driver
#include <WiFiNINA.h>               // Wi‑Fi (ESP32 coprocessor)
#include <WiFiUdp.h>
#include <NTPClient.h>

// ───────── PANEL & PIN DEFINITIONS ─────────
#define PANEL_WIDTH   32
#define BIT_DEPTH      6

uint8_t rgbPins[]  = {7, 8, 9, 10, 11, 12};
uint8_t addrPins[] = {17, 18, 19};

const uint8_t CLK_PIN   = 14;
const int8_t  LAT_PIN   = 15;
const uint8_t OE_PIN    = 16;

Adafruit_Protomatter matrix(
  PANEL_WIDTH, BIT_DEPTH, 1,
  rgbPins, sizeof(addrPins), addrPins,
  CLK_PIN, LAT_PIN, OE_PIN,
  true
);

// ────── WIFI / NTP / SERVER SETUP ──────
const char* SSID     = "1";
const char* PASSWORD = "Spencer1";

const long  UTC_OFFSET = -5 * 3600;
WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET, 60000);

WiFiServer server(80);
int screenMode = 0;  // 0 = clock, 1 = placeholder
int tempValue  = 0;     // last‐set temperature

// ─────────── HELPERS ───────────
void drawCentered(const char *txt) {
  int16_t x1, y1; uint16_t w, h;
  matrix.getTextBounds((char*)txt, 0, 0, &x1, &y1, &w, &h);
  matrix.setCursor((matrix.width() - w) / 2,
                   (matrix.height() - h) / 2);
  matrix.print(txt);
}

// Read one HTTP request and set screenMode
// Replace your handleClient() with this:
void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  // Read request line: "GET /screen?mode=1&temp=23 HTTP/1.1"
  String req = client.readStringUntil('\r');
  client.readStringUntil('\n');  // skip rest of header

  if (req.startsWith("GET /screen?")) {
    // parse mode=
    int mIdx = req.indexOf("mode=");
    if (mIdx >= 0) {
      // take everything after mode= up to '&' or space
      int end = req.indexOf('&', mIdx);
      if (end < 0) end = req.indexOf(' ', mIdx);
      screenMode = req.substring(mIdx + 5, end).toInt();
    }

    // parse temp= if present
    int tIdx = req.indexOf("temp=");
    if (tIdx >= 0) {
      int end = req.indexOf('&', tIdx);
      if (end < 0) end = req.indexOf(' ', tIdx);
      tempValue = req.substring(tIdx + 5, end).toInt();
    }
  }

  // send minimal HTTP response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.print("Mode=");
  client.print(screenMode);
  client.print(", Temp=");
  client.print(tempValue);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ── Wi‑Fi ──
  Serial.print("Connecting to Wi‑Fi");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(" connected!");

  Serial.print("Matrix Portal IP: ");
  Serial.println(WiFi.localIP());

  // ── NTP ──
  timeClient.begin();
  timeClient.update();

  // ── HTTP Server ──
  server.begin();
  Serial.println("HTTP server started");

  // ── Matrix ──
  if (matrix.begin() != PROTOMATTER_OK) {
    Serial.println("Matrix init failed");
    while (1) { delay(10); }
  }
  matrix.setTextWrap(false);
  matrix.setTextColor(matrix.color565(255,255,255));
  matrix.setFont();  // default 5×7
}

void loop() {
  handleClient();          // grab any screen?/mode=…&temp=… requests
  timeClient.update();     // NTP update

  matrix.fillScreen(0);
  if (screenMode == 0) {
    // clock as before
    char buf[6];
    sprintf(buf, "%02d:%02d",
            timeClient.getHours(),
            timeClient.getMinutes());
    drawCentered(buf);

  } else {
    // temperature screen
    char buf[8];
    sprintf(buf, "%dF", tempValue);
    drawCentered(buf);
  }
  matrix.show();
  delay(1000);
}
