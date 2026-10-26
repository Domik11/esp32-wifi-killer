#include "network_diagnostics.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <ESP32Ping.h>
#include "ui_protocol.h"
void printWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n===== WiFi STATUS =====");
    Serial.println("WiFi: Подключено");
    Serial.println("SSID: " + WiFi.SSID());
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println("MAC: " + WiFi.macAddress());
    Serial.println("Шлюз: " + WiFi.gatewayIP().toString());
    Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");

    Serial.print("Ping yandex.ru: ");

    if (Ping.ping("yandex.ru", 3)) {
      uiEvent("status", "\"connected\":true,\"internet\":true,\"ip\":" + jsonString(WiFi.localIP().toString()) + ",\"ping\":" + String(Ping.averageTime()));
      Serial.println("ДОСТУПЕН");
      Serial.println("Средний ping: " +
                     String(Ping.averageTime()) + " ms");
    } else {
      uiEvent("status", "\"connected\":true,\"internet\":false,\"ip\":" + jsonString(WiFi.localIP().toString()));
      Serial.println("НЕДОСТУПЕН");
      refreshNetwork();
    }

  } else {
    uiEvent("status", "\"connected\":false,\"internet\":false");
    Serial.println("\nWiFi: Отключено");
    Serial.println("Код статуса: " + String(WiFi.status()));
    refreshNetwork();
  }
}
