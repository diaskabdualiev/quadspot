#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <Preferences.h>

// Настройки по умолчанию
#define DEFAULT_MIN_PULSE 150    // ~0 градусов
#define DEFAULT_MAX_PULSE 600    // ~180 градусов
#define DEFAULT_CENTER_PULSE 375 // ~90 градусов

// Структура для хранения настроек сервопривода
struct ServoConfig {
  int minPulse;      // Минимальный импульс (0°)
  int maxPulse;      // Максимальный импульс (180°)
  int centerOffset;  // Коррекция центра (90°)
  int currentPos;    // Текущая позиция 
  String name;       // Имя сервопривода
};

class ServoController {
public:
  // Конструктор 
  ServoController(int sda_pin, int scl_pin, uint8_t pca_addr = 0x40);
  
  // Инициализация
  void begin(uint8_t freq = 50);
  
  // Управление сервоприводами
  void setPosition(uint8_t servoIndex, int angle);
  void setAllPositions(int angle);
  int getCurrentPosition(uint8_t servoIndex) const;
  
  // Калибровка
  void calibrateServo(uint8_t servoIndex, int minPulse, int maxPulse, 
                      int centerOffset, const String& name);
  ServoConfig getServoConfig(uint8_t servoIndex) const;
  
  // Доступ к массиву конфигураций
  ServoConfig* getAllServoConfigs();
  uint8_t getServoCount() const;
  
  // Сохранение/загрузка
  void saveSettings();
  void loadSettings();
  
  // Настройка частоты
  void setPWMFrequency(uint8_t freq);
  uint8_t getPWMFrequency() const;
  
private:
  // Внутренние переменные и методы
  Adafruit_PWMServoDriver _pwm;
  ServoConfig _servoConfigs[16];
  Preferences _preferences;
  int _sda_pin, _scl_pin;
  uint8_t _pca_addr, _freq;
  static const uint8_t MAX_SERVOS = 16;
  
  // Преобразование угла в импульс
  int angleToPulse(uint8_t servoIndex, int angle);
  
  // Вспомогательные методы для работы с Preferences
  void saveServoConfigToPreferences(uint8_t servoIndex);
  void loadServoConfigFromPreferences(uint8_t servoIndex);
};

#endif // SERVO_CONTROLLER_H