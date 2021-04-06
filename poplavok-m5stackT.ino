//#include <M5Stack.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiAP.h>
#include <Ticker.h>
#include <Wire.h>
#include <battery.h>

#include "esp_camera.h"
#define CAMERA_MODEL_M5STACK_PSRAM
#include "camera_pins.h"
#include "camera_index.h"

//функции в app_httpd_my.cpp
void startCameraServer();
void setBatteryLed(bool v);
bool getBatteryLed();
void setShineLed(bool v);
bool getShineLed();

//Автор
#define WELCOME_STRING "Poplavok. Protasov AA, 31.08.2020 version 15.03.2021 M5Stack"
//переделка 15 03 2021 на плату M5Stack timer camera https://docs.m5stack.com/#/en/unit/timercam
//среду настраивам как описано тут https://docs.m5stack.com/#/en/quick_start/timer_cam/quick_start_arduino
//ссылка для менеджера плат https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
//плата M5Stack-Timer-CAM
//менеджер библиотек: установить Timer-CAM
//переместить из libraries\Timer-CAM\src в libraries\Timer-CAM\examples\web_cam файл app_httpd.cpp !!!

//Partition scheme No OTA (Large APP)
//выходит 99% пямяти устройства
//инфа тут
// https://github.com/m5stack/M5Stack-Camera
// https://www.youtube.com/watch?v=bIJoVyjTf7g&list=PLDCVbhdMYUfKENkF9pibm3FAfylrL6mrA&index=21&ab_channel=robotzero.one



//точка достпа
#define APNAME "poplavok"
#define DNSNAME "poplavok.local"

//60 сек без подключеных клиентов wifi - спать, миллисекунды
#define SLEEP_TIMEOUT 60000

//Сигнальный светодиод
#define LED_PIN 2
#define LED_ON HIGH
#define LED_OFF LOW

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WiFiServer server(80);

unsigned long previousMillis = 0;

//моргаем когда не спим, секунды
#define HEART_PERIOD 5
Ticker battery;

void callback() {
  //placeholder callback function
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi client connected to my Access Point, uptime: " + String(millis() / 1000) + " sec");
  digitalWrite(LED_PIN, LED_ON);
  delay(500);
  digitalWrite(LED_PIN, LED_OFF);
}

void heartbeart() {
  Serial.print("Wifi clients: ");
  Serial.print(WiFi.softAPgetStationNum());
  Serial.println(", uptime: " + String(millis() / 1000) + " sec");
  Serial.print("Battery level: ");
  Serial.print(bat_get_voltage());
  Serial.println(" V ");
  //если включено свечение поплавка   
  if (getShineLed()) digitalWrite(LED_PIN, LED_ON); else digitalWrite(LED_PIN, LED_OFF);
  //если включено мигание, моргнуть
  if (getBatteryLed()) {
    digitalWrite(LED_PIN, LED_ON ^ getShineLed());
    delay(30);
    digitalWrite(LED_PIN, LED_OFF ^ getShineLed());
  }
}

void toshutdown() {
  digitalWrite(LED_PIN, LED_ON);
  delay(20);
  digitalWrite(LED_PIN, LED_OFF);
  delay(100);
  digitalWrite(LED_PIN, LED_ON);
  delay(20);
  digitalWrite(LED_PIN, LED_OFF);
  delay(100);
  digitalWrite(LED_PIN, LED_ON);
  WiFi.disconnect(); // обрываем WIFI соединения
  WiFi.softAPdisconnect(); // отключаем отчку доступа(если она была
  WiFi.mode(WIFI_OFF); // отключаем WIFI
  delay(100);
  digitalWrite(LED_PIN, LED_OFF);

  Serial.println("I am shutdown, uptime: " + String(millis() / 1000) + " sec.");
  pinMode(LED_PIN, INPUT);

  //Выключить питание от батареи
  bat_disable_output();
  //если питание было по usb - усыпить. пробуждение кнопкой
  esp_deep_sleep_start();
}

void setup() {
  //включить сигнальный светодиод
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON);

  Serial.begin(115200);
  Serial.println(WELCOME_STRING);

  //отключаем точку доступа, для очистки всех настроек wifi
  WiFi.disconnect();
  WiFi.softAPdisconnect();
  WiFi.mode(WIFI_OFF);

  //инициализируем точку доступа
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(APNAME);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, DNSNAME, apIP);

  //измерятель батарейки
  bat_init();
  battery.attach(HEART_PERIOD, heartbeart);

  //M5Stack timer CAM инициализация камеры
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1);//flip it back
  s->set_brightness(s, 1);//up the blightness just a bit
  s->set_saturation(s, -2);//lower the saturation

  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_SVGA);

  startCameraServer();

  Serial.print("Use 'http://");
  Serial.print(WiFi.softAPIP());
  Serial.print("' or 'http://");
  Serial.print(DNSNAME);
  Serial.println("' to connect");
  Serial.println("I am ready !");

  //выключить сигнальный светодиод, инициализация окончена
  digitalWrite(LED_PIN, LED_OFF);
}

void loop() {
  dnsServer.processNextRequest();

  //выключить, если никто не подключился к точке доступа в течении SLEEP_TIMEOUT
  if (WiFi.softAPgetStationNum() > 0) previousMillis = millis(); else if (millis() - previousMillis > SLEEP_TIMEOUT) toshutdown();

}
