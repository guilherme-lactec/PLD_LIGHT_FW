#ifndef DASHBOARD_SERVER_H
#define DASHBOARD_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional> // Para std::function (callbacks)

// Callback para PEDIR dados (ESP32 -> Web)
typedef std::function<void(JsonDocument &doc)> DataCallback;

// ATUALIZADO: Callback para RECEBER dados (Web -> ESP32)
// Agora também recebe o valor de aceleração (0-100)
typedef std::function<void(String ligar, String desligar, int aceleracao)> SettingsCallback;

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
    // --- Handlers HTTP internos ---
    void handleRoot();
    void handleDataJson();
    void handleSettings();

    // --- Membros da Classe ---
    WebServer _server;
    DataCallback _dataCallback;
    SettingsCallback _settingsCallback; // ATUALIZADO: Tipo de callback

    // --- Página HTML ---
    static const char *_dashboard_html;
};

#endif // DASHBOARD_SERVER_H