#include "WiFiProvisioner.h"

// Definição da página HTML (static const)
// ATUALIZADO com CSS e JavaScript para o relógio
const char *WiFiProvisioner::_portal_html = R"EOF(
<!DOCTYPE html>
<html>
<head>
    <title>Configura&ccedil;&atilde;o Wi-Fi ESP32</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 20px; }
        .container { max-width: 400px; margin: auto; padding: 20px; background-color: #fff; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        h2 { text-align: center; color: #333; }
        label { display: block; margin-top: 15px; font-weight: bold; }
        input[type="text"], input[type="password"] { width: calc(100% - 20px); padding: 10px; margin-top: 5px; border: 1px solid #ccc; border-radius: 4px; }
        input[type="submit"] { width: 100%; background-color: #007bff; color: white; padding: 14px 20px; margin-top: 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }
        input[type="submit"]:hover { background-color: #0056b3; }
        /* Estilo para o relógio */
        .clock { text-align: center; font-size: 0.9em; color: #555; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h2>Configurar Wi-Fi (ESP32)</h2>
        <form action="/save" method="POST">
            <label for="ssid">Rede Wi-Fi (SSID):</label>
            <input type="text" id="ssid" name="ssid" required>
            <label for="pass">Senha:</label>
            <input type="password" id="pass" name="pass">
            <input type="submit" value="Salvar e Conectar">
        </form>
        
        <p id="clock" class="clock">Carregando hora local...</p>
    </div>

    <script>
        function updateTime() {
            var now = new Date();
            var options = { 
                weekday: 'long', 
                day: '2-digit', 
                month: 'long', 
                year: 'numeric',
                hour: '2-digit', 
                minute: '2-digit', 
                second: '2-digit' 
            };
            
            // Tenta usar o locale do navegador (ex: pt-BR, pt-PT, en-US)
            var lang = navigator.language || 'pt-PT';
            var timeString = 'Hora local: ' + now.toLocaleString(lang, options);
            
            document.getElementById('clock').innerHTML = timeString;
        }
        // Atualiza agora e depois a cada segundo
        updateTime();
        setInterval(updateTime, 1000);
    </script>
</body>
</html>
)EOF";

WiFiProvisioner::WiFiProvisioner(const char *ap_ssid)
    : _server(80), _ap_ssid(ap_ssid), _ap_ip(192, 168, 4, 1)
{
    // Construtor inicializa a porta do servidor, nome do AP e IP do AP
}

bool WiFiProvisioner::begin()
{

    _reconnectTimer = 0;
    _connectAttempts = 0;
    // Tenta ler as credenciais salvas
    _preferences.begin("wifi-creds", true); // read-only
    _sta_ssid = _preferences.getString("ssid", "");
    _sta_pass = _preferences.getString("pass", "");
    _preferences.end();

    if (_sta_ssid == "" || !startSTAMode())
    {
        // Se não houver credenciais OU a conexão falhar...

        // Se a conexão falhou, limpa as credenciais ruins
        if (_sta_ssid != "")
        {
            Serial.println("Credenciais salvas falharam. Limpando.");
            _preferences.begin("wifi-creds", false); // read-write
            _preferences.clear();
            _preferences.end();
        }

        // Inicia o modo AP para configuração
        startAPMode();
        return false; // Não conectado
    }

    Serial.println("Conexão Wi-Fi estabelecida.");
    return true; // Conectado
}

void WiFiProvisioner::loop()
{
    // --- Modo AP (Portal Cativo) ---
    // Esta parte continua igual
    if (WiFi.getMode() == WIFI_AP)
    {
        _dnsServer.processNextRequest();
        _server.handleClient();
    }
    // --- Modo STA (Conectado à rede) ---
    else if (WiFi.getMode() == WIFI_STA)
    {

        // Se estamos em modo STA, mas não conectados...
        if (WiFi.status() != WL_CONNECTED)
        {

            // E já passou o tempo de tentar de novo (tenta a cada 5 segundos)
            if (millis() - _reconnectTimer > 5000)
            {
                _reconnectTimer = millis(); // Reseta o timer
                _connectAttempts++;

                Serial.printf("[WiFi] Conexão perdida. Tentando reconectar (Tentativa %d)...\n", _connectAttempts);

                // Tenta reconectar de forma NÃO-BLOQUEANTE
                WiFi.reconnect();

                // Se falharmos muitas vezes (ex: 10 vezes),
                // algo está muito errado (ex: senha mudou).
                // Reiniciamos para o modo AP.
                if (_connectAttempts > 10)
                {
                    Serial.println("[WiFi] Muitas falhas. Reiniciando em modo AP.");

                    // Limpa as credenciais ruins antes de reiniciar
                    _preferences.begin("wifi-creds", false);
                    _preferences.clear();
                    _preferences.end();

                    delay(1000);   // Pausa para o Serial enviar a msg
                    ESP.restart(); // O setup() tratará de iniciar o AP
                }
            }
        }
        // Se estamos conectados...
        else
        {
            // Se vínhamos de uma tentativa de reconexão, regista o sucesso
            if (_connectAttempts > 0)
            {
                Serial.println("[WiFi] Reconexão bem-sucedida!");
            }
            // Reseta o contador e o timer
            _connectAttempts = 0;
            _reconnectTimer = millis();
        }
    }
}

bool WiFiProvisioner::isConnected()
{
    return (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED);
}

// --- Funções Privadas ---

bool WiFiProvisioner::startSTAMode()
{
    if (_sta_ssid == "")
        return false;

    Serial.printf("Tentando conectar a: %s\n", _sta_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(_sta_ssid.c_str(), _sta_pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30)
    {
        Serial.print(".");
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n--- CONECTADO ---");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        _server.stop();
        _dnsServer.stop();
        return true;
    }
    else
    {
        Serial.println("\nFalha ao conectar.");
        WiFi.disconnect(true);
        return false;
    }
}

void WiFiProvisioner::startAPMode()
{
    Serial.println("Iniciando Modo AP (Hotspot).");

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(_ap_ip, _ap_ip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(_ap_ssid.c_str());

    _dnsServer.start(53, "*", _ap_ip);

    Serial.printf("AP SSID: %s\n", _ap_ssid.c_str());
    Serial.printf("AP IP: %s\n", _ap_ip.toString().c_str());

    // --- Rotas do Servidor Web ---
    // Usamos std::bind para ligar os métodos da classe aos callbacks
    _server.on("/", HTTP_GET, std::bind(&WiFiProvisioner::handleRoot, this));
    _server.on("/save", HTTP_POST, std::bind(&WiFiProvisioner::handleSave, this));
    _server.onNotFound(std::bind(&WiFiProvisioner::handleNotFound, this));

    _server.begin();
    Serial.println("Servidor Web e DNS iniciados.");
}

void WiFiProvisioner::handleRoot()
{
    _server.send(200, "text/html", _portal_html);
}

void WiFiProvisioner::handleSave()
{
    Serial.println("Recebendo credenciais...");

    _sta_ssid = _server.arg("ssid");
    _sta_pass = _server.arg("pass");

    _preferences.begin("wifi-creds", false);
    _preferences.putString("ssid", _sta_ssid);
    _preferences.putString("pass", _sta_pass);
    _preferences.end();

    String response = "<html><body><h2>Credenciais salvas!</h2><p>O ESP32 ir&aacute; reiniciar e tentar se conectar &agrave; rede <b>" + _sta_ssid + "</b>.</p></body></html>";
    _server.send(200, "text/html", response);

    Serial.println("Credenciais salvas. Reiniciando em 3 segundos...");
    delay(3000);
    ESP.restart();
}

void WiFiProvisioner::handleNotFound()
{
    _server.sendHeader("Location", "http://" + _ap_ip.toString(), true);
    _server.send(302, "text/plain", ""); // 302 Redirect
}