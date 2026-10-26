#include "device_storage.h"
#include <Preferences.h>
#include "ui_protocol.h"
// Имена пространства и ключей совместимы с исходной прошивкой.
bool saveDevice(const Device& device) {
  Preferences prefs;
  if (!prefs.begin("lan-device", false)) { Serial.println("Ошибка открытия NVS"); return false; }
  bool ok = prefs.putString("ip", device.ip.toString()) > 0;
  ok = (prefs.putString("mac", device.mac) > 0) && ok;
  ok = (prefs.putString("name", device.name) > 0) && ok;
  prefs.end();
  Serial.println(ok ? "Устройство сохранено!" : "Ошибка записи NVS");
  if (ok) uiEvent("saved", "\"ip\":" + jsonString(device.ip.toString()) + ",\"mac\":" + jsonString(device.mac) + ",\"name\":" + jsonString(device.name));
  else uiEvent("error", "\"message\":\"Ошибка записи устройства в NVS.\"");
  return ok;
}
bool loadDevice(Device& device) {
  Preferences prefs;
  if (!prefs.begin("lan-device", true)) return false;
  String ip = prefs.getString("ip", "");
  device.mac = prefs.getString("mac", "");
  device.name = prefs.getString("name", "Unknown");
  prefs.end();
  device.ip.fromString(ip);
  // Даже при устаревшем/повреждённом IP устройство можно найти по идентификатору.
  return device.mac.length() > 0 ||
    (device.name.length() > 0 && !device.name.equalsIgnoreCase("Unknown"));
}
void showSavedDevice() {
  Device device;
  if (!loadDevice(device)) { Serial.println("Сохранённых устройств нет или запись некорректна."); return; }
  Serial.printf("Сохранённое устройство: %s | %s | %s\n", device.ip.toString().c_str(), device.mac.c_str(), device.name.c_str());
}
