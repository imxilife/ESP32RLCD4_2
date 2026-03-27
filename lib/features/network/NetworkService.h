#pragma once

#include <Arduino.h>

namespace NetworkService {
void begin();
bool isConnected();
String ipAddress();
String ssid();
}
