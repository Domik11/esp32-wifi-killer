#include <Preferences.h>
#include "ip_manager.h"
#include "network_config.h"
#include <WiFi.h>
namespace {
uint32_t number(IPAddress ip) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value = (value << 8) | ip[i];
  return value;
}
bool unicast(IPAddress ip) { return ip[0] != 0 && ip[0] != 127 && ip[0] < 224; }
}
bool configureStaticIP(IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns) {
  const uint32_t m = number(mask), host = ~m, a = number(ip), g = number(gateway);
  if (!m || host < 3 || (host & (host + 1)) || !unicast(ip) || !unicast(gateway) || !unicast(dns) ||
      (a & m) != (g & m) || !(a & host) || (a & host) == host ||
      !(g & host) || (g & host) == host || a == g) {
    Serial.println("Некорректные статические сетевые настройки."); return false;
  }
  // Проверка синтаксиса не доказывает, что адрес свободен: резервируйте его на маршрутизаторе.
  if (!WiFi.config(ip, gateway, mask, dns)) { Serial.println("Ошибка WiFi.config"); return false; }
  return true;
}
void initializeIP() {
  if (!USE_STATIC_IP) return;
  IPAddress ip, gateway, mask, dns;
  if (!ip.fromString(STATIC_IP) || !gateway.fromString(STATIC_GATEWAY) ||
      !mask.fromString(STATIC_MASK) || !dns.fromString(STATIC_DNS) ||
      !configureStaticIP(ip, gateway, mask, dns)) {
    Serial.println("Статический IP не применён; остановка для исправления настроек.");
    while (true) delay(1000);
  }
}

// По запросу пользователя восстановлено исходное назначение сохранённого IP.
void setSavedIP() {
  Preferences prefs;
  prefs.begin("lan-device", true);

  String savedIP = prefs.getString("ip", "");

  prefs.end();

  if (savedIP.isEmpty()) {
    Serial.println("Сохранённый IP не найден!");
    return;
  }

  IPAddress newIP;

  if (!newIP.fromString(savedIP)) {
    Serial.println("Некорректный IP!");
    return;
  }

  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress dns = WiFi.dnsIP(0);

  if (WiFi.config(newIP, gateway, subnet, dns)) {
    Serial.print("Новый IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Ошибка изменения IP!");
  }
}
