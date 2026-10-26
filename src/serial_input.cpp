#include "serial_input.h"
String readLine() {
  // Дождаться целой строки, а не принимать паузу ввода за её конец.
  String value;
  bool overflow = false;
  for (;;) {
    if (!Serial.available()) { delay(5); continue; }
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n') break;
    if (value.length() < 256) value += ch;
    else overflow = true;
  }
  if (overflow) { Serial.println("Строка слишком длинная."); return ""; }
  if (value.endsWith("\r")) value.remove(value.length() - 1);

  return value;
}

String readTrimmedLine() {
  String value = readLine();
  value.trim();
  return value;
}
