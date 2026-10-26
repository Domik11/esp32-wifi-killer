#include "wifi_manager.h"
#include "serial_input.h"
#include "ui_protocol.h"
#include <WiFi.h>
namespace {
// Не зависит от WiFi.SSID(), который после отключения может быть пустым.
String selectedSSID;
String selectedPassword; // Только RAM: пароль вводится через USB, не записывается в NVS.
bool selectedOpen = false;
uint32_t lastReconnectAttempt = 0;
bool reconnectAttempted = false;
constexpr uint32_t RECONNECT_INTERVAL_MS = 15000;
}

void refreshNetwork() {
  if (selectedSSID.isEmpty()) {
    Serial.println("Сеть ещё не выбрана. Сначала вызовите connectWiFi().");
    return;
  }
  // Диагностика вызывает функцию каждую секунду при потере связи.
  // Не обрываем уже запущенную попытку подключения каждым вызовом.
  const uint32_t now = millis();
  if (reconnectAttempted && now - lastReconnectAttempt < RECONNECT_INTERVAL_MS) return;
  reconnectAttempted = true;
  lastReconnectAttempt = now;
  Serial.println("Переподключение WiFi к " + selectedSSID + "...");
  WiFi.disconnect(false, false);
  WiFi.begin(selectedSSID.c_str(), selectedOpen ? nullptr : selectedPassword.c_str());
}
void connectWiFi() {
  Serial.println("\nПоиск Wi-Fi сетей...");
  uiEvent("wifi_scan");

  int n = WiFi.scanNetworks();

  if (n <= 0) {
    Serial.println("Сети не найдены.");
    uiEvent("error", "\"message\":\"Сети не найдены. Повтор поиска.\"");
    WiFi.scanDelete();
    delay(2000);
    return;
  }

  for (int i = 0; i < n; i++) {
    uiEvent("wifi", "\"index\":" + String(i + 1) + ",\"ssid\":" + jsonString(WiFi.SSID(i)) +
      ",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"open\":" +
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false"));
    Serial.printf(
      "%d: %s (%d dBm) %s\n",
      i + 1,
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      WiFi.encryptionType(i) == WIFI_AUTH_OPEN
        ? "OPEN" : "*"
    );
  }

  Serial.println("\nВыберите сеть:");
  uiRequest("wifi_choice");

  int choice = readLine().toInt();

  if (choice < 1 || choice > n) {
    Serial.println("Неверный номер сети.");
    uiEvent("error", "\"message\":\"Выберите сеть из списка.\"");
    WiFi.scanDelete();
    return;
  }

  String ssid = WiFi.SSID(choice - 1);
  selectedSSID = ssid;
  selectedOpen = WiFi.encryptionType(choice - 1) == WIFI_AUTH_OPEN;
  reconnectAttempted = false;

  WiFi.scanDelete();

  selectedPassword = "";
  if (!selectedOpen) {
    Serial.println("Введите пароль Wi-Fi:");
    uiRequest("wifi_password");
    selectedPassword = readLine();
    bool validPassword = selectedPassword.length() >= 8 && selectedPassword.length() <= 63;
    if (selectedPassword.length() == 64) {
      validPassword = true;
      for (size_t i = 0; i < selectedPassword.length(); ++i) {
        const char ch = selectedPassword[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
          validPassword = false;
          break;
        }
      }
    }
    if (!validPassword) {
      Serial.println("Некорректная длина пароля Wi-Fi.");
      uiEvent("error", "\"message\":\"Пароль должен содержать 8–63 байта или 64 hex-символа.\"");
      return;
    }
  }

  Serial.print("Подключение");

  uiEvent("wifi_connecting", "\"ssid\":" + jsonString(selectedSSID));
  WiFi.begin(selectedSSID.c_str(), selectedOpen ? nullptr : selectedPassword.c_str());

  unsigned long started = millis();

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - started > 15000) {
      Serial.println("\nОшибка подключения.");
      uiEvent("error", "\"message\":\"Не удалось подключиться: проверьте пароль и сигнал сети.\"");
      WiFi.disconnect();
      return;
    }

    Serial.print(".");
    delay(500);
  }

  Serial.println("\nПодключено!");
  uiEvent("wifi_connected", "\"ssid\":" + jsonString(WiFi.SSID()) + ",\"ip\":" + jsonString(WiFi.localIP().toString()));

  Serial.println("\nСетевые настройки:");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Mask: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("DNS 1: ");
  Serial.println(WiFi.dnsIP(0));

  Serial.print("DNS 2: ");
  Serial.println(WiFi.dnsIP(1));

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}
