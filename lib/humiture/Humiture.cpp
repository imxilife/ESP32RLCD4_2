#include "Humiture.h"

Humiture::Humiture() : initialized_(false) {
}

bool Humiture::begin(int sda, int scl) {
    if (!shtc3_.begin()) {
        Serial.println("SHTC3 初始化失败！请检查接线:");
        Serial.println("  - SDA: GPIO13");
        Serial.println("  - SCL: GPIO14");
        Serial.println("  - VCC: 3.3V");
        Serial.println("  - GND: GND");
        initialized_ = false;
        return false;
    }
    
    Serial.println("SHTC3 初始化成功！");
    initialized_ = true;
    return true;
}

float Humiture::readTemperature() {
    if (!initialized_) {
        return 0.0f;
    }
    
    sensors_event_t humidity, temp;
    shtc3_.getEvent(&humidity, &temp);
    return temp.temperature;
}

float Humiture::readHumidity() {
    if (!initialized_) {
        return 0.0f;
    }
    
    sensors_event_t humidity, temp;
    shtc3_.getEvent(&humidity, &temp);
    return humidity.relative_humidity;
}

bool Humiture::read(float &temperature, float &humidity) {
    if (!initialized_) {
        return false;
    }
    
    sensors_event_t humidityEvent, tempEvent;
    shtc3_.getEvent(&humidityEvent, &tempEvent);
    
    temperature = tempEvent.temperature;
    humidity = humidityEvent.relative_humidity;
    
    return true;
}

bool Humiture::isInitialized() const {
    return initialized_;
}
