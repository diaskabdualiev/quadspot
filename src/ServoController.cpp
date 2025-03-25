#include "ServoController.h"

// Конструктор
ServoController::ServoController(int sda_pin, int scl_pin, uint8_t pca_addr) 
  : _sda_pin(sda_pin), _scl_pin(scl_pin), _pca_addr(pca_addr), _pwm(pca_addr), _freq(50) {
}

// Инициализация
void ServoController::begin(uint8_t freq) {
  // Инициализация I2C
  Wire.begin(_sda_pin, _scl_pin);
  
  // Инициализация PCA9685
  _pwm.begin();
  _pwm.setPWMFreq(freq);
  _freq = freq;
  
  // Инициализация настроек сервоприводов по умолчанию
  for (uint8_t i = 0; i < MAX_SERVOS; i++) {
    _servoConfigs[i].minPulse = DEFAULT_MIN_PULSE;
    _servoConfigs[i].maxPulse = DEFAULT_MAX_PULSE;
    _servoConfigs[i].centerOffset = 0;
    _servoConfigs[i].currentPos = 90;  // По умолчанию центр
    _servoConfigs[i].name = "Servo " + String(i + 1);
  }
  
  // Загрузка сохраненных настроек, если они есть
  loadSettings();
  
  // Центрирование всех сервоприводов
  for (uint8_t i = 0; i < MAX_SERVOS; i++) {
    setPosition(i, 90);
    delay(10);  // Небольшая задержка между инициализацией сервоприводов
  }
}

// Преобразование угла в импульс с учетом калибровки
int ServoController::angleToPulse(uint8_t servoIndex, int angle) {
  // Проверка границ
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  
  // Сохраняем новую позицию
  _servoConfigs[servoIndex].currentPos = angle;
  
  // Применяем коррекцию центра только для позиции точно 90 градусов
  if (angle == 90) {
    return map(angle, 0, 180, _servoConfigs[servoIndex].minPulse, 
              _servoConfigs[servoIndex].maxPulse) + _servoConfigs[servoIndex].centerOffset;
  }
  
  // Для остальных углов используем линейное отображение
  // Для углов < 90, используем минимальный импульс и скорректированный центр
  if (angle < 90) {
    return map(angle, 0, 90, 
               _servoConfigs[servoIndex].minPulse, 
               (_servoConfigs[servoIndex].minPulse + _servoConfigs[servoIndex].maxPulse) / 2 + 
               _servoConfigs[servoIndex].centerOffset);
  } 
  // Для углов > 90, используем скорректированный центр и максимальный импульс
  else {
    return map(angle, 90, 180, 
               (_servoConfigs[servoIndex].minPulse + _servoConfigs[servoIndex].maxPulse) / 2 + 
               _servoConfigs[servoIndex].centerOffset,
               _servoConfigs[servoIndex].maxPulse);
  }
}

// Установка позиции сервопривода
void ServoController::setPosition(uint8_t servoIndex, int angle) {
  if (servoIndex < MAX_SERVOS) {
    int pulse = angleToPulse(servoIndex, angle);
    _pwm.setPWM(servoIndex, 0, pulse);
  }
}

// Установка одинаковой позиции для всех сервоприводов
void ServoController::setAllPositions(int angle) {
  for (uint8_t i = 0; i < MAX_SERVOS; i++) {
    setPosition(i, angle);
  }
}

// Получение текущей позиции сервопривода
int ServoController::getCurrentPosition(uint8_t servoIndex) const {
  if (servoIndex < MAX_SERVOS) {
    return _servoConfigs[servoIndex].currentPos;
  }
  return 0;
}

// Калибровка сервопривода
void ServoController::calibrateServo(uint8_t servoIndex, int minPulse, 
                                    int maxPulse, int centerOffset, 
                                    const String& name) {
  if (servoIndex < MAX_SERVOS) {
    _servoConfigs[servoIndex].minPulse = minPulse;
    _servoConfigs[servoIndex].maxPulse = maxPulse;
    _servoConfigs[servoIndex].centerOffset = centerOffset;
    _servoConfigs[servoIndex].name = name;
    
    // Сохраняем калибровку сразу в память
    saveServoConfigToPreferences(servoIndex);
    
    // Обновляем позицию сервопривода
    setPosition(servoIndex, _servoConfigs[servoIndex].currentPos);
  }
}

// Получение конфигурации сервопривода
ServoConfig ServoController::getServoConfig(uint8_t servoIndex) const {
  if (servoIndex < MAX_SERVOS) {
    return _servoConfigs[servoIndex];
  }
  
  // Возвращаем конфигурацию по умолчанию, если индекс вне диапазона
  ServoConfig defaultConfig;
  defaultConfig.minPulse = DEFAULT_MIN_PULSE;
  defaultConfig.maxPulse = DEFAULT_MAX_PULSE;
  defaultConfig.centerOffset = 0;
  defaultConfig.currentPos = 90;
  defaultConfig.name = "Invalid";
  return defaultConfig;
}

// Получение указателя на массив конфигураций всех сервоприводов
ServoConfig* ServoController::getAllServoConfigs() {
  return _servoConfigs;
}

// Получение количества сервоприводов
uint8_t ServoController::getServoCount() const {
  return MAX_SERVOS;
}

// Настройка частоты PWM
void ServoController::setPWMFrequency(uint8_t freq) {
  if (freq >= 40 && freq <= 1000) {  // Устанавливаем разумные ограничения
    _pwm.setPWMFreq(freq);
    _freq = freq;
    
    // Сохраняем частоту в настройки
    _preferences.begin("servo-config", false);
    _preferences.putUInt("freq", freq);
    _preferences.end();
  }
}

// Получение частоты PWM
uint8_t ServoController::getPWMFrequency() const {
  return _freq;
}

// Сохранение настроек конкретного сервопривода в память
void ServoController::saveServoConfigToPreferences(uint8_t servoIndex) {
  _preferences.begin("servo-config", false);
  
  String prefix = "servo" + String(servoIndex) + "_";
  char keyMin[20], keyMax[20], keyCenter[20], keyName[20];
  
  // Формируем ключи
  (prefix + "min").toCharArray(keyMin, sizeof(keyMin));
  (prefix + "max").toCharArray(keyMax, sizeof(keyMax));
  (prefix + "center").toCharArray(keyCenter, sizeof(keyCenter));
  (prefix + "name").toCharArray(keyName, sizeof(keyName));
  
  _preferences.putInt(keyMin, _servoConfigs[servoIndex].minPulse);
  _preferences.putInt(keyMax, _servoConfigs[servoIndex].maxPulse);
  _preferences.putInt(keyCenter, _servoConfigs[servoIndex].centerOffset);
  _preferences.putString(keyName, _servoConfigs[servoIndex].name);
  
  _preferences.end();
}

// Загрузка настроек конкретного сервопривода из памяти
void ServoController::loadServoConfigFromPreferences(uint8_t servoIndex) {
  _preferences.begin("servo-config", true);  // true = только для чтения
  
  String prefix = "servo" + String(servoIndex) + "_";
  char keyMin[20], keyMax[20], keyCenter[20], keyName[20];
  
  // Формируем ключи
  (prefix + "min").toCharArray(keyMin, sizeof(keyMin));
  (prefix + "max").toCharArray(keyMax, sizeof(keyMax));
  (prefix + "center").toCharArray(keyCenter, sizeof(keyCenter));
  (prefix + "name").toCharArray(keyName, sizeof(keyName));
  
  _servoConfigs[servoIndex].minPulse = _preferences.getInt(keyMin, DEFAULT_MIN_PULSE);
  _servoConfigs[servoIndex].maxPulse = _preferences.getInt(keyMax, DEFAULT_MAX_PULSE);
  _servoConfigs[servoIndex].centerOffset = _preferences.getInt(keyCenter, 0);
  _servoConfigs[servoIndex].name = _preferences.getString(keyName, "Servo " + String(servoIndex + 1));
  
  _preferences.end();
}

// Сохранение всех настроек в память
void ServoController::saveSettings() {
  _preferences.begin("servo-config", false);
  
  // Устанавливаем флаг наличия настроек
  _preferences.putBool("hasSettings", true);
  
  // Сохраняем частоту PWM
  _preferences.putUInt("freq", _freq);
  
  _preferences.end();
  
  // Сохраняем настройки каждого сервопривода
  for (uint8_t i = 0; i < MAX_SERVOS; i++) {
    saveServoConfigToPreferences(i);
  }
  
  Serial.println("Настройки сервоприводов сохранены в память");
}

// Загрузка всех настроек из памяти
void ServoController::loadSettings() {
  _preferences.begin("servo-config", true);  // true = только для чтения
  
  // Проверяем, есть ли сохраненные настройки
  bool hasSettings = _preferences.getBool("hasSettings", false);
  if (!hasSettings) {
    _preferences.end();
    Serial.println("Сохраненных настроек не найдено, используем значения по умолчанию");
    return;
  }
  
  // Загружаем частоту PWM
  _freq = _preferences.getUInt("freq", 50);
  _pwm.setPWMFreq(_freq);
  
  _preferences.end();
  
  // Загружаем настройки каждого сервопривода
  for (uint8_t i = 0; i < MAX_SERVOS; i++) {
    loadServoConfigFromPreferences(i);
  }
  
  Serial.println("Настройки сервоприводов загружены из памяти");
}