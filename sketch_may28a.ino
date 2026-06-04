#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid     = "Julia123";
const char* password = "qwerty123";
const char* ntfyTopic = "CareHome_IrkutskIoT";

const int pirPin  = 18;
const int reedPin = 19;

const unsigned long noMotionTimeout  = 20000;
const unsigned long noMotionCooldown = 30000;

int lastPirState  = -1;
int lastReedState = -1;

unsigned long lastMotionTime      = 0;
unsigned long lastNoMotionMsgTime = 0;
unsigned long doorClosedTime      = 0;

bool noMotionAlertSent  = false;
bool personHome         = true;
bool waitingAfterDoor   = false; 

void sendNotification(String title, String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi не подключен");
    return;
  }
  WiFiClient client;
  HTTPClient http;
  String url = String("http://ntfy.sh/") + ntfyTopic;
  if (http.begin(client, url)) {
    http.addHeader("Title", title);
    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    int httpCode = http.POST(message);
    Serial.print("ntfy ответ: ");
    Serial.println(httpCode);
    http.end();
  } else {
    Serial.println("Ошибка подключения к ntfy.sh");
  }
}

void connectWiFi() {
  Serial.print("Подключение к Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi подключен! IP: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  pinMode(pirPin,  INPUT);
  pinMode(reedPin, INPUT);
  connectWiFi();
  Serial.println("Ожидание прогрева PIR датчика (30 сек)...");
  delay(30000);
  lastPirState  = digitalRead(pirPin);
  lastReedState = digitalRead(reedPin);
  lastMotionTime = millis();
  sendNotification("CareHome запущена", "Система мониторинга активна и готова к работе.");
  Serial.println("Система запущена. Мониторинг начат.");
}

void loop() {
  int pirState  = digitalRead(pirPin);
  int reedState = digitalRead(reedPin);
  unsigned long now = millis();

  if (pirState != lastPirState) {
    Serial.println(pirState == HIGH ? "Движение обнаружено" : "Движения нет");
    lastPirState = pirState;
  }

  if (pirState == HIGH && personHome) {
    lastMotionTime = now;
    noMotionAlertSent = false;

    if (waitingAfterDoor) {
      waitingAfterDoor = false;
      Serial.println("Движение после двери — человек дома");
    }
  }

  if (reedState != lastReedState) {
    if (reedState == LOW) {
      Serial.println("Дверь открылась");
    } else {
      if (!personHome) {
        personHome = true;
        noMotionAlertSent = false;
        lastMotionTime = now;
        waitingAfterDoor = false;
        Serial.println("Человек вернулся домой");
        sendNotification("Информация", "Человек вернулся домой. Мониторинг возобновлён.");
      } else {
        waitingAfterDoor = true;
        doorClosedTime = now;
        Serial.println("Дверь закрылась — ожидаем движение 20 сек...");
      }
    }
    lastReedState = reedState;
  }

  if (waitingAfterDoor && (now - doorClosedTime >= noMotionTimeout)) {
    waitingAfterDoor = false;
    personHome = false;
    noMotionAlertSent = true;
    Serial.println("Человек ушёл из дома");
    sendNotification("Информация", "Человек ушёл из дома. Мониторинг приостановлен.");
  }

  if (personHome && !waitingAfterDoor && !noMotionAlertSent &&
      (now - lastMotionTime >= noMotionTimeout)) {
    if (now - lastNoMotionMsgTime >= noMotionCooldown) {
      sendNotification("Внимание!", "Долго нет движения. Проверьте состояние человека.");
      lastNoMotionMsgTime = now;
      noMotionAlertSent = true;
    }
  }

  delay(200);
}
