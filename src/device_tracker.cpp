#include "device_tracker.h"
#include "device_storage.h"
#include "network_scanner.h"
#include "ip_manager.h"
#include "ui_protocol.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
void trackerTask(void*) {
  Device saved;
  if (!loadDevice(saved)) {
    Serial.println("Нет сохранённого устройства для поиска.");
    vTaskDelete(nullptr);
    return;
  }
  bool reportedFound = false;
  bool wasConnected = false;
  uint32_t lastSearch = 0;
  for (;;) {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected) { wasConnected = false; reportedFound = false; }
    const uint32_t interval = reportedFound ? 30000 : 5000;
    if (connected && (!wasConnected || millis() - lastSearch >= interval)) {
      wasConnected = true;
      Device found;
      if (findSavedDevice(saved, found)) {
        const bool changed = found.ip != saved.ip || !found.mac.equalsIgnoreCase(saved.mac);
        if (!reportedFound || changed) {
          uiEvent("tracked", "\"found\":true,\"ip\":" + jsonString(found.ip.toString()) + ",\"mac\":" + jsonString(found.mac));
          Serial.printf("Сохранённое устройство найдено: %s | %s | %s\n",
                        saved.name.c_str(), found.mac.c_str(), found.ip.toString().c_str());
        }
        // Пользовательское имя не заменяется DNS-именем.
        found.name = saved.name;
        if (!changed || saveDevice(found)) {
          saved = found;
          reportedFound = true;
          // Автоматическое присвоение оставлено по явному выбору пользователя.
          // Применяем только после обнаружения и успешного сохранения адреса.
          if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != saved.ip) setSavedIP();
        }
        else reportedFound = false; // Повторить запись при следующем поиске.
      } else {
        uiEvent("tracked", "\"found\":false");
        reportedFound = false;
        if (WiFi.status() == WL_CONNECTED)
          Serial.println("Сохранённое устройство пока не найдено; поиск будет повторён.");
      }
      lastSearch = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
}

void startDeviceTracker() {
  static bool started = false;
  if (started) return;
  // Сканирование и PTR могут ждать ответы, поэтому они не выполняются в loop.
  started = xTaskCreate(trackerTask, "device-tracker", 8192, nullptr, 1, nullptr) == pdPASS;
  if (!started) Serial.println("Не удалось запустить поиск сохранённого устройства.");
}
