#ifndef WIFI_PROVISIONER_H
#define WIFI_PROVISIONER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <functional> // Necessário para std::bind

class WiFiProvisioner
{
public:
    /**
     * @brief Construtor da classe.
     * @param ap_ssid O nome do hotspot (AP) que será criado para configuração.
     */
    WiFiProvisioner(const char *ap_ssid = "ESP32-Config");

    /**
     * @brief Inicia o gerenciador. Tenta conectar ao WiFi salvo.
     * Se falhar, inicia o modo AP (portal cativo).
     * @return true se conectou ao WiFi (STA), false se iniciou o modo AP.
     */
    bool begin();

    /**
     * @brief Função de loop. Deve ser chamada em cada loop() do sketch principal.
     * Gerencia o servidor web/DNS (modo AP) ou a reconexão (modo STA).
     */
    void loop();

    /**
     * @brief Verifica se o ESP32 está conectado à rede Wi-Fi.
     * @return true se conectado, false caso contrário.
     */
    bool isConnected();

private:
    // --- Funções de lógica interna ---
    void startAPMode();
    bool startSTAMode();

    // --- Handlers do Servidor Web (Callbacks) ---
    void handleRoot();
    void handleSave();
    void handleNotFound();

    // --- Objetos de gerenciamento ---
    WebServer _server;
    DNSServer _dnsServer;
    Preferences _preferences;

    // --- Variáveis de estado ---
    String _sta_ssid;
    String _sta_pass;
    String _ap_ssid;
    IPAddress _ap_ip;

    // --- Página HTML ---
    static const char *_portal_html;

    unsigned long _reconnectTimer; // Timer para reconexão
    int _connectAttempts;
};

#endif // WIFI_PROVISIONER_H