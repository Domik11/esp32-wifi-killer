#pragma once
// DHCP по умолчанию. Задайте отдельный свободный адрес ESP32 вне DHCP-пула.
constexpr bool USE_STATIC_IP = false;
constexpr const char* STATIC_IP = "192.0.2.10";
constexpr const char* STATIC_GATEWAY = "192.0.2.1";
constexpr const char* STATIC_MASK = "255.255.255.0";
constexpr const char* STATIC_DNS = "192.0.2.1";
