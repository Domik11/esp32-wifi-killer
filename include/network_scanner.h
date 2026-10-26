#pragma once
#include "device.h"
void scanLAN();
bool selectDevice();
// Возвращает свежую запись по сохранённому MAC, имя — только при отсутствии MAC.
bool findSavedDevice(const Device& saved, Device& found);
