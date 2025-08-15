#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

const char* WIFI_SSID = "Sakibmob";
const char* WIFI_PASS = "12345698";
WebServer server(80);

#define LED_GPIO_NUM 4
unsigned long lastRequestTime = 0;
const unsigned long LED_TIMEOUT = 5000; // 5 seconds timeout

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

// MJPEG stream boundary
#define PART_BOUNDARY "123456789000000000000987654321"se
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

void serveJpg()
{
  digitalWrite(LED_GPIO_NUM, HIGH); // Turn on LED
  lastRequestTime = millis(); // Update request time

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    digitalWrite(LED_GPIO_NUM, LOW); // Turn off LED on failure
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi()
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid()
{
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

void handleMjpeg()
{
  digitalWrite(LED_GPIO_NUM, HIGH); // Turn on LED
  lastRequestTime = millis();

  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
    server.send(503, "", "");
    digitalWrite(LED_GPIO_NUM, LOW);
    return;
  }

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, _STREAM_CONTENT_TYPE, "");

  WiFiClient client = server.client();
  while (client.connected()) {
    lastRequestTime = millis(); // Update request time
    auto frame = esp32cam::capture();
    if (frame == nullptr) {
      Serial.println("STREAM CAPTURE FAIL");
      continue;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), _STREAM_PART, frame->size());
    client.write(_STREAM_BOUNDARY);
    client.write(buf);
    frame->writeTo(client);
    delay(10); // Small delay to avoid overwhelming the client
  }
  digitalWrite(LED_GPIO_NUM, LOW); // Turn off LED when client disconnects
}

void setup() {
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW); // LED off initially

  Serial.begin(115200);
  Serial.println();
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(midRes); // Default to mid resolution
    cfg.setBufferCount(2);
    cfg.setJpeg(60); // Lower JPEG quality for faster streaming

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /stream");

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);
  server.on("/stream", handleMjpeg); // New MJPEG stream endpoint

  server.begin();
}

void loop() {
  server.handleClient();

  // Turn off LED if no requests for LED_TIMEOUT milliseconds
  if (millis() - lastRequestTime > LED_TIMEOUT) {
    digitalWrite(LED_GPIO_NUM, LOW);
  }
}