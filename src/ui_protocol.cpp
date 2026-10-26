#include "ui_protocol.h"
String jsonString(const String& value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t ch = value[i];
    if (ch == '"' || ch == '\\') { result += '\\'; result += char(ch); }
    else if (ch < 32) { char hex[7]; snprintf(hex, sizeof(hex), "\\u%04x", ch); result += hex; }
    else result += char(ch);
  }
  return result + '"';
}
void uiEvent(const char* event, const String& fields) {
  // Одна запись в UART: сообщения задач не разрывают JSON на части.
  String line = "\n@ESP32 {\"event\":" + jsonString(event);
  if (fields.length()) line += "," + fields;
  line += "}\n";
  Serial.print(line);
}
void uiRequest(const char* field) { uiEvent("request", "\"field\":" + jsonString(field)); }
