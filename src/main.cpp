#include <Arduino.h>
#include "ServoController.h"
#include "WebServerManager.h"

// Пины I2C и адрес PCA9685
#define I2C_SDA 21
#define I2C_SCL 22
#define PCA9685_ADDR 0x40

// Объекты для управления
ServoController servoController(I2C_SDA, I2C_SCL, PCA9685_ADDR);
WebServerManager webServerManager(&servoController);

// Буфер для команд Serial
String serialCommand = "";
bool commandComplete = false;

// Обработка команд с последовательного порта
void processSerialCommand() {
  serialCommand.trim();
  
  if (serialCommand.equals("calibration")) {
    Serial.println("Включение режима калибровки...");
    if (webServerManager.startCalibrationMode()) {
      Serial.println("Режим калибровки активирован");
    } else {
      Serial.println("Ошибка при запуске режима калибровки");
    }
  }
  else if (serialCommand.equals("working")) {
    Serial.println("Переключение в рабочий режим...");
    if (webServerManager.isCalibrationMode()) {
      webServerManager.stopCalibrationMode();
      Serial.println("Рабочий режим активирован");
    } else {
      Serial.println("Система уже в рабочем режиме");
    }
  }
  else if (serialCommand.equals("status")) {
    if (webServerManager.isCalibrationMode()) {
      Serial.println("Текущий режим: КАЛИБРОВКА");
      if (WiFi.getMode() == WIFI_AP) {
        Serial.print("IP адрес (AP): ");
        Serial.println(WiFi.softAPIP());
      } else if (WiFi.getMode() == WIFI_STA) {
        Serial.print("IP адрес (STA): ");
        Serial.println(WiFi.localIP());
      }
    } else {
      Serial.println("Текущий режим: РАБОЧИЙ");
    }
  }
  else if (serialCommand.equals("save")) {
    Serial.println("Сохранение всех настроек...");
    servoController.saveSettings();
  }
  else if (serialCommand.equals("reset")) {
    Serial.println("Перезагрузка устройства...");
    ESP.restart();
  }
  else if (serialCommand.equals("help") || serialCommand.equals("?")) {
    Serial.println("\n--- Доступные команды ---");
    Serial.println("calibration - Включить режим калибровки (WiFi и веб-интерфейс)");
    Serial.println("working     - Переключиться в рабочий режим (выключить WiFi)");
    Serial.println("status      - Показать текущий статус");
    Serial.println("save        - Сохранить все настройки в память");
    Serial.println("reset       - Перезагрузить устройство");
    Serial.println("help или ?  - Показать эту справку");
  }
  else if (serialCommand.length() > 0) {
    Serial.println("Неизвестная команда: " + serialCommand);
    Serial.println("Введите 'help' для справки");
  }
  
  serialCommand = "";
  commandComplete = false;
}

// Функция для обратной кинематики (ПРИМЕР)
void inverseKinematics() {
  // Здесь будет ваш код для обратной кинематики
  // Пример:
  
  // static unsigned long lastUpdate = 0;
  // unsigned long currentTime = millis();
  
  // if (currentTime - lastUpdate > 50) {  // Обновление каждые 50 мс
  //   lastUpdate = currentTime;
  //   
  //   // Пример расчета позиций для обратной кинематики
  //   // и установка позиций сервоприводов
  //   // servoController.setPosition(0, angle1);
  //   // servoController.setPosition(1, angle2);
  //   // и т.д.
  // }
}

void setup() {
  // Инициализация Serial с высокой скоростью для быстрого отклика
  Serial.begin(115200);
  
  // Небольшая задержка для стабилизации последовательного порта
  delay(500);
  
  Serial.println("\n-----------------------------------");
  Serial.println("Система управления сервоприводами");
  Serial.println("-----------------------------------");
  
  // Инициализация контроллера сервоприводов
  servoController.begin(50);
  Serial.println("Контроллер сервоприводов инициализирован");
  
  // Инициализация веб-сервера
  if (webServerManager.begin()) {
    if (webServerManager.isCalibrationMode()) {
      Serial.println("Запущен в режиме калибровки");
    } else {
      Serial.println("Запущен в рабочем режиме");
    }
  }
  
  Serial.println("\nВведите 'help' для справки по командам");
}

void loop() {
  // Чтение команд с последовательного порта
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (serialCommand.length() > 0) {
        commandComplete = true;
      }
    } else {
      serialCommand += inChar;
    }
  }
  
  // Обработка команды, если получена
  if (commandComplete) {
    processSerialCommand();
  }
  
  // Обслуживание веб-сервера в режиме калибровки
  webServerManager.update();
  
  // Если НЕ в режиме калибровки - выполняем алгоритмы обратной кинематики
  if (!webServerManager.isCalibrationMode()) {
    inverseKinematics();
  }
}