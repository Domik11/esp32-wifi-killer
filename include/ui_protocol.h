#pragma once
#include <Arduino.h>
String jsonString(const String& value);
void uiEvent(const char* event, const String& fields = "");
void uiRequest(const char* field);
