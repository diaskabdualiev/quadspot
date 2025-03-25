#include "WebServerManager.h"

// Инициализация статической переменной-указателя
WebServerManager* WebServerManager::_instance = nullptr;

// Конструктор
WebServerManager::WebServerManager(ServoController* servoController)
  : _servoController(servoController), 
    _server(80), 
    _ws("/ws"),
    _calibrationMode(false),
    _apMode(true),
    _ssid("AlashElectronics"),
    _password("28071917"),
    _wsClient(nullptr) {
  // Сохраняем указатель на экземпляр для использования в статических методах
  _instance = this;
}

// Инициализация
bool WebServerManager::begin() {
  // Инициализация SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return false;
  }
  
  // Загружаем сохраненный режим
  _calibrationMode = loadMode();
  
  // Если режим калибровки, запускаем WiFi и веб-сервер
  if (_calibrationMode) {
    return startCalibrationMode(_ssid.c_str(), _password.c_str(), _apMode);
  }
  
  return true;
}

// Запуск режима калибровки
bool WebServerManager::startCalibrationMode(const char* ssid, const char* password, bool apMode) {
  _ssid = ssid;
  _password = password;
  _apMode = apMode;
  
  // Настройка WiFi
  if (_apMode) {
    // Режим точки доступа
    WiFi.softAP(_ssid.c_str(), _password.c_str());
    Serial.print("Access Point started. IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    // Режим подключения к существующей сети
    WiFi.begin(_ssid.c_str(), _password.c_str());
    Serial.print("Statiom Point started.");
    // Ждем подключения с таймаутом 20 секунд
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nFailed to connect to WiFi");
      return false;
    }
    
    Serial.println("\nConnected to WiFi. IP Address: ");
    Serial.println(WiFi.localIP());
  }
  
  // Настройка веб-сервера
  setupWebServer();
  
  // Сохраняем режим
  _calibrationMode = true;
  saveMode(_calibrationMode);
  
  return true;
}

// Остановка режима калибровки
void WebServerManager::stopCalibrationMode() {
  // Останавливаем веб-сервер и отключаем WiFi
  _server.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  // Сохраняем режим
  _calibrationMode = false;
  saveMode(_calibrationMode);
  
  Serial.println("Калибровочный режим остановлен");
}

// Проверка текущего режима
bool WebServerManager::isCalibrationMode() const {
  return _calibrationMode;
}

// Настройка веб-сервера и обработчиков
void WebServerManager::setupWebServer() {
  // Настройка WebSocket
  _ws.onEvent(onWebSocketEvent);
  _server.addHandler(&_ws);
  
  // Главная страница
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  // Обработчик статических файлов
  _server.serveStatic("/", SPIFFS, "/");
  
  // Запуск сервера
  _server.begin();
  
  Serial.println("Web server started");
}

// Периодические задачи
void WebServerManager::update() {
  if (_calibrationMode) {
    _ws.cleanupClients();
  }
}

// Статический обработчик WebSocket событий (передает управление экземпляру класса)
void WebServerManager::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (_instance) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), 
                     client->remoteIP().toString().c_str());
        // Сохраняем указатель на клиент
        _instance->_wsClient = client;
        // Отправляем текущую конфигурацию
        _instance->sendCurrentConfig(client);
        break;
        
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        // Если это был наш активный клиент, сбрасываем указатель
        if (_instance->_wsClient == client) {
          _instance->_wsClient = nullptr;
        }
        break;
        
      case WS_EVT_DATA:
        // Обновляем указатель на клиент и обрабатываем сообщение
        _instance->_wsClient = client;
        _instance->handleWebSocketMessage(client, arg, data, len);
        break;
        
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
    }
  }
}

// Обработка WebSocket сообщений
void WebServerManager::handleWebSocketMessage(AsyncWebSocketClient* client, 
                                           void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // Добавляем нуль-терминатор для корректной работы с строкой
    String message = String((char*)data);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    
    // Обработка команд
    String command = doc["command"];
    
    if (command == "getConfig") {
      sendCurrentConfig(client);
    }
    else if (command == "setPosition") {
      int servoIndex = doc["servoIndex"];
      int angle = doc["angle"];
      _servoController->setPosition(servoIndex, angle);
      
      // Подтверждение установки позиции
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "positionSet";
      respDoc["servoIndex"] = servoIndex;
      respDoc["angle"] = angle;
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "calibrate") {
      int servoIndex = doc["servoIndex"];
      
      if (doc["minPulse"].is<int>()) {
        int minPulse = doc["minPulse"];
        int maxPulse = _servoController->getServoConfig(servoIndex).maxPulse;
        int centerOffset = _servoController->getServoConfig(servoIndex).centerOffset;
        String name = _servoController->getServoConfig(servoIndex).name;
        
        if (doc["maxPulse"].is<int>()) {
          maxPulse = doc["maxPulse"];
        }
        if (doc["centerOffset"].is<int>()) {
          centerOffset = doc["centerOffset"];
        }
        if (doc["name"].is<const char*>()) {
          name = doc["name"].as<String>();
        }
        
        _servoController->calibrateServo(servoIndex, minPulse, maxPulse, centerOffset, name);
      }
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "calibrated";
      respDoc["servoIndex"] = servoIndex;
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "setAllPositions") {
      JsonArray positions = doc["positions"];
      uint8_t index = 0;
      
      for (JsonVariant value : positions) {
        if (index < _servoController->getServoCount()) {
          _servoController->setPosition(index, value.as<int>());
          index++;
        }
      }
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "allPositionsSet";
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "centerAll") {
      _servoController->setAllPositions(90);
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "allCentered";
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "minAll") {
      _servoController->setAllPositions(0);
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "allMin";
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "maxAll") {
      _servoController->setAllPositions(180);
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "allMax";
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "setFrequency") {
      int freq = doc["frequency"];
      _servoController->setPWMFrequency(freq);
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "frequencySet";
      respDoc["frequency"] = freq;
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
    else if (command == "saveSettings") {
      _servoController->saveSettings();
      
      JsonDocument respDoc;
      respDoc["status"] = "ok";
      respDoc["command"] = "settingsSaved";
      
      String response;
      serializeJson(respDoc, response);
      client->text(response);
    }
  }
}

// Отправка текущей конфигурации клиенту
void WebServerManager::sendCurrentConfig(AsyncWebSocketClient* client) {
  JsonDocument doc;
  
  doc["servos"] = JsonArray();
  JsonArray servos = doc["servos"].to<JsonArray>();
  
  for (uint8_t i = 0; i < _servoController->getServoCount(); i++) {
    ServoConfig config = _servoController->getServoConfig(i);
    JsonObject servo = servos.add<JsonObject>();
    servo["index"] = i;
    servo["name"] = config.name;
    servo["minPulse"] = config.minPulse;
    servo["maxPulse"] = config.maxPulse;
    servo["centerOffset"] = config.centerOffset;
    servo["currentPos"] = config.currentPos;
  }
  
  doc["frequency"] = _servoController->getPWMFrequency();
  
  String response;
  serializeJson(doc, response);
  
  client->text(response);
}

// Сохранение режима в энергонезависимую память
void WebServerManager::saveMode(bool calibrationMode) {
  _preferences.begin("web-config", false);
  _preferences.putBool("cal_mode", calibrationMode);
  _preferences.end();
}

// Загрузка режима из энергонезависимой памяти
bool WebServerManager::loadMode() {
  _preferences.begin("web-config", true);
  bool mode = _preferences.getBool("cal_mode", false);
  _preferences.end();
  return mode;
}