#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "ServoController.h"

class WebServerManager {
public:
  // Конструктор получает ссылку на контроллер сервоприводов
  WebServerManager(ServoController* servoController);
  
  // Инициализация
  bool begin();
  
  // Управление режимами
  bool startCalibrationMode(const char* ssid = "AlashElectronics", 
                          const char* password = "28071917", 
                          bool apMode = false);
  void stopCalibrationMode();
  bool isCalibrationMode() const;
  
  // Обработка периодических задач
  void update();
  
private:
  // Внутренние переменные
  ServoController* _servoController;
  AsyncWebServer _server;
  AsyncWebSocket _ws;
  Preferences _preferences;
  bool _calibrationMode;
  bool _apMode;
  String _ssid, _password;
  AsyncWebSocketClient* _wsClient;
  
  // Настройка веб-сервера и обработчики
  void setupWebServer();
  static void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                             AwsEventType type, void* arg, uint8_t* data, size_t len);
  void handleWebSocketMessage(AsyncWebSocketClient* client, void* arg, 
                            uint8_t* data, size_t len);
  void sendCurrentConfig(AsyncWebSocketClient* client);
  void saveMode(bool calibrationMode);
  bool loadMode();
  
  // Указатель на экземпляр для использования в статическом методе
  static WebServerManager* _instance;
};

#endif // WEB_SERVER_MANAGER_H