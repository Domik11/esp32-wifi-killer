#pragma once
#include <IPAddress.h>
bool configureStaticIP(IPAddress ip, IPAddress gateway, IPAddress mask, IPAddress dns);
void initializeIP();
void setSavedIP();
