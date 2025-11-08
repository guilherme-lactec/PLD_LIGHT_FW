#include <Arduino.h>
#include "WiFiProvisioner.h" // Nossa biblioteca
#include "time.h"            // Para NTP
#include <WebServer.h>       // Para o novo servidor de dashboard
#include <ArduinoJson.h>     // Para enviar dados para a página

// --- Configuração da Biblioteca de Provisionamento ---
WiFiProvisioner provisioner("ESP32-Config");

// --- Servidor Web do Dashboard ---
WebServer dashboardServer(80); // O novo servidor que rodará na porta 80

// --- Configuração do NTP ---
const char *ntpServer = "a.st1.ntp.br";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

// --- Variáveis de Controle ---
bool ntpInitialized = false;
unsigned long lastSerialPrint = 0;
const unsigned long SERIAL_PRINT_INTERVAL = 10000; // 10 segundos

// --- DADOS FICTÍCIOS ---
// Substitua isso pela leitura do seu sensor real
float sensorFicticio = 25.0;

// =================================================================
// Página HTML do Dashboard
// Esta é a página que você verá no seu navegador
// =================================================================
const char *dashboard_html = R"EOF(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 0; }
        .container { max-width: 600px; margin: 30px auto; padding: 20px; background-color: #fff; border-radius: 10px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        h1 { text-align: center; color: #333; }
        .card { background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 8px; padding: 15px; margin-top: 20px; }
        .card h2 { margin-top: 0; color: #0056b3; }
        .data { font-size: 2.5em; font-weight: bold; color: #333; text-align: center; margin: 10px 0; }
        #sensor { color: #d9534f; }
        #time { color: #5cb85c; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Dashboard</h1>
        
        <div class="card">
            <h2>Data & Hora (NTP)</h2>
            <div id="date" class="data">--/--/----</div>
            <div id="time" class="data">--:--:--</div>
        </div>

        <div class="card">
            <h2>Sensor (Exemplo)</h2>
            <div id="sensor" class="data">--.-- &deg;C</div>
        </div>
    </div>

    <script>
        // Função para buscar dados do ESP32
        function fetchData() {
            // Faz um pedido para o endpoint /data.json
            fetch('/data.json')
                .then(response => response.json())
                .then(data => {
                    // Atualiza os elementos HTML com os novos dados
                    document.getElementById('date').innerText = data.date;
                    document.getElementById('time').innerText = data.time;
                    // Formata o sensor para 1 casa decimal
                    document.getElementById('sensor').innerHTML = parseFloat(data.sensor).toFixed(1) + ' &deg;C';
                })
                .catch(error => {
                    console.error('Erro ao buscar dados:', error);
                    document.getElementById('date').innerText = "Erro";
                    document.getElementById('time').innerText = "Erro";
                    document.getElementById('sensor').innerText = "Erro";
                });
        }

        // Chama a função pela primeira vez
        fetchData();
        
        // Configura para chamar a função a cada 2 segundos
        setInterval(fetchData, 1000);
    </script>
</body>
</html>
)EOF";

// =================================================================
// Funções de Callback do Servidor do Dashboard
// =================================================================

/**
 * @brief Envia a página HTML principal (o dashboard)
 */
void handleDashboardRoot()
{
  dashboardServer.send(200, "text/html", dashboard_html);
}

/**
 * @brief Envia os dados (hora e sensor) em formato JSON
 */
void handleDataJson()
{
  // 1. Simula a leitura de um sensor (substitua isso)
  sensorFicticio += random(-5, 6) / 10.0; // Variação de +/- 0.5
  if (sensorFicticio < 10)
    sensorFicticio = 10;
  if (sensorFicticio > 40)
    sensorFicticio = 40;

  // 2. Obtém a hora
  struct tm timeinfo;
  char dateStr[20];
  char timeStr[20];

  if (getLocalTime(&timeinfo))
  {
    // Hora sincronizada
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  }
  else
  {
    // Hora ainda não sincronizada
    strcpy(dateStr, "Sincronizando...");
    strcpy(timeStr, "--:--:--");
  }

  // 3. Cria o documento JSON
  StaticJsonDocument<256> doc;
  doc["date"] = dateStr;
  doc["time"] = timeStr;
  doc["sensor"] = sensorFicticio;

  // 4. Serializa o JSON e envia
  String output;
  serializeJson(doc, output);
  dashboardServer.send(200, "application/json", output);
}

/**
 * @brief Configura e inicia o servidor do dashboard.
 */
void setupDashboardServer()
{
  dashboardServer.on("/", HTTP_GET, handleDashboardRoot);
  dashboardServer.on("/data.json", HTTP_GET, handleDataJson);
  dashboardServer.begin();
  Serial.println("Servidor de Dashboard iniciado!");
  Serial.print("Acesse o dashboard em: http://");
  Serial.println(WiFi.localIP());
}

// =================================================================
// Funções Principais (setup e loop)
// =================================================================

void initNTP()
{
  Serial.println("Configurando NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora...");
}

void printSerialStatus()
{
  // Imprime o status no Serial Monitor (para debug)
  struct tm timeinfo;
  Serial.println("---------------------------------");
  Serial.println("Status: Conectado");
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());

  if (getLocalTime(&timeinfo, 100))
  { // Tenta obter a hora (timeout 100ms)
    if (!ntpInitialized)
      ntpInitialized = true;
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%A, %d/%m/%Y %H:%M:%S", &timeinfo);
    Serial.printf("  Hora: %s\n", buffer);
  }
  else if (!ntpInitialized)
  {
    Serial.println("  Hora: ...aguardando sincronia NTP...");
  }
  else
  {
    Serial.println("  Hora: Falha temporária ao obter hora.");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nIniciando...");

  if (provisioner.begin())
  {
    // --- Conectado com sucesso ---
    initNTP();
    setupDashboardServer(); // Inicia o *novo* servidor
  }
  else
  {
    // --- Em modo de configuração ---
    Serial.println("Iniciado em modo AP para configuração.");
    Serial.println("Conecte-se à rede 'ESP32-Config'.");
  }
}

void loop()
{
  // A biblioteca de provisionamento *sempre* roda no loop
  // (no modo AP, ela cuida do portal; no modo STA, ela cuida da reconexão)
  provisioner.loop();

  // O código abaixo só roda se estivermos conectados ao Wi-Fi
  if (provisioner.isConnected())
  {

    // Processa requisições do servidor do dashboard
    dashboardServer.handleClient();

    // Imprime o status no Serial a cada 10 segundos
    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL)
    {
      printSerialStatus();
      lastSerialPrint = millis();
    }
  }

  delay(10);
}