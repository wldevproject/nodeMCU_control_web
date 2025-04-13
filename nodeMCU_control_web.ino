#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFiManager.h>  // Untuk WiFiManager
#include <ArduinoJson.h>

#define IN1 D1  // Motor A+
#define IN2 D2  // Motor A-
#define IN3 D5  // Motor B+
#define IN4 D6  // Motor B-

// set untuk ap-mode
const char* ssid = "ESP-KENDALI";
const char* password = "12345678";

ESP8266WebServer server(80);

// Ganti dengan SSID dan PASS prioritas
const char* primarySSID = "SSID_KAMU";
const char* primaryPASS = "PASSWORD_KAMU";

// IP statis (ubah sesuai kebutuhan)
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

int speedPWM = 0;
const int maxSpeed = 1023;
const int stepSpeed = 50;

unsigned long lastStopTime = 0;
bool isStopping = false;
String lastDirection = "q";

extern const char* htmlPage;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);  // Mulai dalam mode station
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(primarySSID, primaryPASS);
  Serial.println("Mencoba konek ke WiFi...");

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nTerhubung ke WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nGagal konek, mengaktifkan AP mode...");
    WiFi.mode(WIFI_AP);  // Ganti ke mode AP
    WiFi.softAP(ssid, password);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/move", []() {
    String dir = server.arg("dir");
    int spd = server.hasArg("spd") ? server.arg("spd").toInt() : 0;

    if (dir == "q") {
      isStopping = true;
      lastStopTime = millis();
    } else {
      speedPWM = spd;
      gerak(dir, speedPWM);
      isStopping = false;
    }

    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Server dimulai. Gunakan browser atau Serial Monitor.");
}

void loop() {
  server.handleClient();

  // Deselerasi perlahan
  if (isStopping && speedPWM > 0 && millis() - lastStopTime >= 50) {
    speedPWM -= stepSpeed;
    if (speedPWM < 0) speedPWM = 0;
    gerak(lastDirection, speedPWM);
    lastStopTime = millis();
  }

  // Kendali via Serial Monitor
  if (Serial.available()) {
    char key = tolower(Serial.read());
    if (String("wasdq").indexOf(key) >= 0) {
      if (key == 'q') {
        isStopping = true;
        lastStopTime = millis();
      } else {
        speedPWM += stepSpeed;
        if (speedPWM > maxSpeed) speedPWM = maxSpeed;
        gerak(String(key), speedPWM);
        isStopping = false;
      }
    }
  }
}

// Fungsi gerak motor berdasarkan arah dan kecepatan
void gerak(String arah, int spd) {
  spd = constrain(spd, 0, maxSpeed);
  lastDirection = arah;

  if (arah == "w") {
    analogWrite(IN1, spd);
    analogWrite(IN2, 0);
    analogWrite(IN3, spd);
    analogWrite(IN4, 0);
  } else if (arah == "s") {
    analogWrite(IN1, 0);
    analogWrite(IN2, spd);
    analogWrite(IN3, 0);
    analogWrite(IN4, spd);
  } else if (arah == "d") {
    analogWrite(IN1, 0);
    analogWrite(IN2, spd);
    analogWrite(IN3, spd);
    analogWrite(IN4, 0);
  } else if (arah == "a") {
    analogWrite(IN1, spd);
    analogWrite(IN2, 0);
    analogWrite(IN3, 0);
    analogWrite(IN4, spd);
  } else if (arah == "wd") {
    analogWrite(IN1, 0);
    analogWrite(IN2, spd / 2);
    analogWrite(IN3, spd);
    analogWrite(IN4, 0);
  } else if (arah == "wa") {
    analogWrite(IN1, spd / 2);
    analogWrite(IN2, 0);
    analogWrite(IN3, 0);
    analogWrite(IN4, spd);
  } else if (arah == "sd") {
    analogWrite(IN1, 0);
    analogWrite(IN2, spd / 2);
    analogWrite(IN3, 0);
    analogWrite(IN4, spd);
  } else if (arah == "sa") {
    analogWrite(IN1, 0);
    analogWrite(IN2, spd);
    analogWrite(IN3, 0);
    analogWrite(IN4, spd / 2);
  } else {
    analogWrite(IN1, 0);
    analogWrite(IN2, 0);
    analogWrite(IN3, 0);
    analogWrite(IN4, 0);
  }
}

// Halaman HTML untuk kontrol via browser
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Kendali Motor</title>
  <style>
    body { font-family: sans-serif; text-align: center; padding-top: 30px; }
    h1 { color: #333; }
  </style>
</head>
<body>
  <h1>Gunakan Keyboard: W A S D untuk gerak, Q untuk stop</h1>
  <p>Tekan dan tahan untuk mempercepat!</p>
  <script>
    let speed = 0;
    const maxSpeed = 1023;
    const step = 50;
    let interval = null;
    const keys = new Set();

    function updateDirection() {
      const dir = Array.from(keys).sort().join('');
      fetch(`/move?dir=${dir}&spd=${speed}`);
    }

    document.addEventListener("keydown", (e) => {
      const key = e.key.toLowerCase();
      if ("wasd".includes(key)) {
        keys.add(key);
        if (!interval) {
          interval = setInterval(() => {
            if (speed < maxSpeed) speed += step;
            updateDirection();
          }, 100);
        }
      }
    });

    document.addEventListener("keyup", (e) => {
      const key = e.key.toLowerCase();
      keys.delete(key);
      if (keys.size === 0) {
        clearInterval(interval);
        interval = null;
        speed = 0;
        fetch("/move?dir=q");
      }
    });
  </script>
</body>
</html>
)rawliteral";
