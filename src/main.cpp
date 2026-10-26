#include <Arduino.h>
#include <WiFi.h>
#include "wifi_manager.h"
#include "network_scanner.h"
#include "ip_manager.h"
#include "device_storage.h"
#include "network_diagnostics.h"
#include "device_tracker.h"
#include "ui_protocol.h"

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10000);
  WiFi.persistent(false); // Новые учётные данные не записывать во flash.
  WiFi.mode(WIFI_STA);
  delay(1000);
  uiEvent("hello", "\"protocol\":1");
  showSavedDevice();
  // До успешного выбора и записи нового устройства фоновые операции не запускаются.
  for (;;) {
    while (WiFi.status() != WL_CONNECTED) connectWiFi();
    scanLAN();
    if (selectDevice()) break;
    Serial.println("Устройство не выбрано или не сохранено. Повтор сканирования...");
    delay(1000);
  }
  // setSavedIP вызывается трекером после подтверждения текущего адреса устройства.
  startDeviceTracker();
  uiEvent("ready");
}

void loop() {
  printWiFiStatus();
  delay(1000);
}
