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

  WiFiManager wifiManager;

  Serial.begin(115200);
  LittleFS.begin();

  String ssidFromFile = "";
  String passFromFile = "";

  // Baca SSID dan password tersimpan dari LittleFS
  if (LittleFS.exists("/wifi.json")) {
    File file = LittleFS.open("/wifi.json", "r");
    if (file) {
      DynamicJsonDocument doc(256);
      if (deserializeJson(doc, file) == DeserializationError::Ok) {
        ssidFromFile = doc["ssid"].as<String>();
        passFromFile = doc["pass"].as<String>();
      }
      file.close();
    }
  }

  WiFi.mode(WIFI_STA);  // Mulai dalam mode station
  WiFi.config(local_IP, gateway, subnet);
  // WiFi.begin(primarySSID, primaryPASS);
  // Serial.println("Mencoba konek ke WiFi...");

  // Kalau SSID tersimpan ada, coba konek langsung
  if (ssidFromFile != "") {
    WiFi.begin(ssidFromFile.c_str(), passFromFile.c_str());
    Serial.println("Mencoba konek ke SSID tersimpan...");

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
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password);
      Serial.print("IP Address: ");
      Serial.println(WiFi.softAPIP());
    }
  } else {
    // Kalau belum ada data, masuk WiFiManager
    if (!wifiManager.autoConnect("ESP-KENDALI", "12345678")) {
      Serial.println("Gagal connect. Restart...");
      delay(3000);
      ESP.restart();
    }
  }

  // Form WiFi
  server.on("/wifi", []() {
    String html = R"rawliteral(
      <!DOCTYPE html>
      <html><head><title>WiFi Setup</title></head>
      <body>
        <h1>Setup WiFi</h1>
        <form action="/save-ssid" method="POST">
          <input type="text" name="ssid" placeholder="SSID WiFi" required><br>
          <input type="password" name="password" placeholder="Password WiFi" required><br>
          <button type="submit">Simpan & Hubungkan</button>
        </form>
      </body>
      </html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  // Simpan SSID dan password, lalu restart ESP
  server.on("/save-ssid", HTTP_POST, []() {
    String newSSID = server.arg("ssid");
    String newPASS = server.arg("password");

    DynamicJsonDocument doc(256);
    doc["ssid"] = newSSID;
    doc["pass"] = newPASS;

    File file = LittleFS.open("/wifi.json", "w");
    if (file) {
      serializeJson(doc, file);
      file.close();
    }

    String html = "<h2>Berhasil disimpan! Restarting...</h2>";
    server.send(200, "text/html", html);
    delay(2000);
    ESP.restart();
  });

  // Halaman kontrol
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });


  // Endpoint untuk kendali motor
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
  Serial.println("Server dimulai di IP: " + WiFi.localIP().toString());
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
  <h3 style="color:green;">Terhubung ke IP: <span id="ip"></span></h3>

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

    document.getElementById('ip').innerText = location.hostname;

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
