#pragma once
#include <Arduino.h>
#include <IPAddress.h>
constexpr int MAX_DEVICES = 64;
struct Device { IPAddress ip; String mac; String name; };
