#ifndef DASHBOARD_SERVER_H
#define DASHBOARD_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional> // Para std::function (callbacks)

typedef std::function<void(JsonDocument &doc)> DataCallback;

// ATUALIZADO: Callback para RECEBER dados (Web -> ESP32)
// Trocamos 'aceleracao' por 'luzMaxima'
typedef std::function<void(String ligar, String desligar, int luzMaxima)> SettingsCallback;

class DashboardServer
{
public:
    DashboardServer(int port = 80);
    void begin();
    void loop();
    void onDataRequest(DataCallback callback);

    /**
     * @brief ATUALIZADO: Registra a função que será chamada ao salvar configurações.
     */
    void onSettingsRequest(SettingsCallback callback);

private:
    void handleRoot();
    void handleDataJson();
    void handleSettings();

    WebServer _server;
    DataCallback _dataCallback;
    SettingsCallback _settingsCallback; // ATUALIZADO: Tipo de callback

    static const char *_dashboard_html;
};

#endif // DASHBOARD_SERVER_H