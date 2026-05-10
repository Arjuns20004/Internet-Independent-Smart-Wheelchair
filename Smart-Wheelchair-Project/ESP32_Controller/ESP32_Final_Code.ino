#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ESP32Servo.h>

/* ================= DHT ================= */
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

/* ================= UART TO UNO ================= */
HardwareSerial uno(2);

/* ================= WEB ================= */
WebServer server(80);

/* ================= JOYSTICK ================= */
#define JOY_X 34
#define JOY_Y 35

/* ================= TOUCH ================= */
#define TOUCH1 26
#define TOUCH2 25
#define TOUCH3 14
#define TOUCH4 27

/* ================= RAIN ================= */
#define RAIN 13
Servo rainServo;

/* ================= STATE ================= */
char currentCommand = 'S';
char lastSent = 'X';

/* ================================================= */

void setup() {

  Serial.begin(115200);

  uno.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.softAP("Wheelchair", "12345678");

  server.on("/", handleRoot);
  server.on("/cmd", handleCommand);
  server.begin();

  dht.begin();

  pinMode(RAIN, INPUT);

  pinMode(TOUCH1, INPUT);
  pinMode(TOUCH2, INPUT);
  pinMode(TOUCH3, INPUT);
  pinMode(TOUCH4, INPUT);

  rainServo.attach(12);

  Serial.println("ESP32 READY");
}

/* ================================================= */

void loop() {

  server.handleClient();

  updateJoystick();
  updateTouch();

  sendIfChanged();
  rainControl();
}

/* ================= SEND DATA ================= */

void sendIfChanged() {

  if (currentCommand != lastSent) {

    uno.write(currentCommand);

    Serial.print("Sent: ");
    Serial.println(currentCommand);

    lastSent = currentCommand;
  }
}

/* ================= JOYSTICK FIXED ================= */

void updateJoystick() {

  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  char newCmd = 'S';

  // Forward
  if (y < 1000)
    newCmd = 'F';

  // Backward
  else if (y > 4000)
    newCmd = 'L';

  // Right
  else if (x < 1000)
    newCmd = 'R';

  // Left (your real case: bottom-right corner)
  else if (x > 4000 && y > 4000)
    newCmd = 'B';

  else
    newCmd = 'S';

  currentCommand = newCmd;

  Serial.print("X=");
  Serial.print(x);

  Serial.print(" Y=");
  Serial.print(y);

  Serial.print(" CMD=");
  Serial.println(newCmd);
}

/* ================= TOUCH HOLD MODE ================= */

void updateTouch() {

  if (digitalRead(TOUCH1) == HIGH)
    currentCommand = 'F';

  else if (digitalRead(TOUCH2) == HIGH)
    currentCommand = 'B';

  else if (digitalRead(TOUCH3) == HIGH)
    currentCommand = 'L';

  else if (digitalRead(TOUCH4) == HIGH)
    currentCommand = 'R';
}

/* ================= RAIN ================= */

void rainControl() {

  if (digitalRead(RAIN) == LOW)
    rainServo.write(90);
  else
    rainServo.write(0);
}

/* ================= WEB ================= */

void handleCommand() {

  if (server.hasArg("move")) {

    currentCommand = server.arg("move")[0];

    sendIfChanged();
  }

  server.send(200, "text/plain", "OK");
}

/* ================= WEB UI ================= */

void handleRoot() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  String rainStatus = (digitalRead(RAIN) == LOW) ? "RAIN" : "NO RAIN";

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{background:#111;color:white;text-align:center;font-family:Arial;}";
  html += ".card{background:#222;margin:10px;padding:15px;border-radius:15px;}";
  html += "button{width:90%;height:55px;margin:8px;font-size:20px;border-radius:12px;}";
  html += "</style></head><body>";

  html += "<h2>SMART WHEELCHAIR</h2>";

  html += "<div class='card'>";
  html += "Temp: " + String(t) + " C<br>";
  html += "Humidity: " + String(h) + " %<br>";
  html += "Rain: " + rainStatus + "<br>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<button onclick=\"send('F')\">Forward</button>";
  html += "<button onclick=\"send('B')\">Backward</button>";
  html += "<button onclick=\"send('L')\">Left</button>";
  html += "<button onclick=\"send('R')\">Right</button>";
  html += "<button onclick=\"send('S')\">Stop</button>";
  html += "</div>";

  html += "<script>";
  html += "function send(m){fetch('/cmd?move='+m);}";
  html += "</script>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}
