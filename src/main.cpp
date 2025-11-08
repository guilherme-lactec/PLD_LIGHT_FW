#include <Arduino.h>
#include "WiFiProvisioner.h"
#include "DashboardServer.h"
#include "time.h"
#include <ArduinoJson.h>
#include <Preferences.h> // *** NVS *** Biblioteca para memória não volátil

// --- Configuração das Bibliotecas ---
WiFiProvisioner provisioner("ESP32-Config");
DashboardServer dashboardServer(80);
Preferences preferences; // *** NVS *** Objeto para salvar/ler dados

// --- Configuração do NTP ---
const char *ntpServer = "a.st1.ntp.br";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

// --- Variáveis de Controle ---
bool ntpInitialized = false;
unsigned long lastSerialPrint = 0;
const unsigned long SERIAL_PRINT_INTERVAL = 10000;

// =========================================================
// --- DADOS DO SEU PROJETO (SENSORES E ESTADO) ---
// Sensores
float tempFicticia = 25.0;
float humFicticia = 60.0;
int lumFicticia = 800;

// Configurações
String horaLigar;    // Agora são inicializadas no setup()
String horaDesligar; // Agora são inicializadas no setup()
int aceleracaoSalva; // Agora é inicializada no setup()
// =========================================================

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
  {
    if (!ntpInitialized)
      ntpInitialized = true;
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%A, %d/%m/%Y %H:%M:%S", &timeinfo);
    Serial.printf("  Hora: %s\n", buffer);
  }
  else
  {
    Serial.println("  Hora: ...aguardando sincronia NTP...");
  }

  Serial.printf("  Sensores: Temp=%.1f C, Hum=%.1f %%, Lum=%d lux\n",
                tempFicticia, humFicticia, lumFicticia);
  Serial.printf("  Config: Luz Ligar=%s, Desligar=%s, Acel=%d\n",
                horaLigar.c_str(), horaDesligar.c_str(), aceleracaoSalva);
}

// Lógica de simulação (ou leitura real) dos sensores
void atualizarSensores()
{
  tempFicticia += random(-5, 6) / 10.0;
  humFicticia += random(-10, 11) / 10.0;
  lumFicticia += random(-50, 51);

  if (tempFicticia < 15)
    tempFicticia = 15;
  if (humFicticia < 30)
    humFicticia = 30;
  if (lumFicticia < 100)
    lumFicticia = 100;
}

void checkLightSchedule()
{
  // Lógica para ligar/desligar a luz
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nIniciando...");

  // *** NVS ***
  // Carrega as configurações salvas
  // Usamos "app-settings" como "espaço" (namespace)
  preferences.begin("app-settings", false);

  // Tenta ler os valores. Se não existirem, usa os valores padrão (o 2º argumento).
  horaLigar = preferences.getString("horaLigar", "18:00");
  horaDesligar = preferences.getString("horaDesligar", "06:00");
  aceleracaoSalva = preferences.getInt("acelSalva", 50);

  preferences.end(); // Fecha as preferências (boa prática)

  Serial.println("Configurações carregadas da NVS:");
  Serial.printf("  Ligar: %s, Desligar: %s, Acel: %d\n", horaLigar.c_str(), horaDesligar.c_str(), aceleracaoSalva);

  if (provisioner.begin())
  {
    // --- Conectado com sucesso ---
    initNTP();

    // CALLBACK 1: O que o ESP32 ENVIA para a web (GET)
    dashboardServer.onDataRequest([](JsonDocument &doc)
                                  {
            
            atualizarSensores(); // Atualiza leituras

            // Adiciona os dados dos SENSORES ao JSON
            doc["temperatura"] = tempFicticia;
            doc["humidade"] = humFicticia;
            doc["luminosidade"] = lumFicticia;
            
            // Adiciona os dados das CONFIGURAÇÕES (lidas da NVS) ao JSON
            doc["hora_ligar"] = horaLigar;       
            doc["hora_desligar"] = horaDesligar; 
            doc["aceleracao"] = aceleracaoSalva; });

    // CALLBACK 2: O que o ESP32 RECEBE da web (POST)
    dashboardServer.onSettingsRequest([](String ligar, String desligar, int aceleracao)
                                      {
            
            // 1. Salva os valores recebidos nas variáveis globais (RAM)
            horaLigar = ligar;
            horaDesligar = desligar;
            aceleracaoSalva = aceleracao;

            // 2. *** NVS *** Salva os novos valores na memória não volátil
            preferences.begin("app-settings", false); // Abre para escrita
            preferences.putString("horaLigar", horaLigar);
            preferences.putString("horaDesligar", horaDesligar);
            preferences.putInt("acelSalva", aceleracaoSalva);
            preferences.end(); // Fecha

            // Imprime no Serial para confirmar
            Serial.println("\n!!! NOVAS CONFIGURAÇÕES SALVAS NA NVS !!!");
            Serial.printf("Ligar às: %s\n", horaLigar.c_str());
            Serial.printf("Desligar às: %s\n", horaDesligar.c_str());
            Serial.printf("Aceleração: %d\n\n", aceleracaoSalva); });

    // Inicia o servidor do dashboard
    dashboardServer.begin();
    Serial.print("Acesse o dashboard em: http://");
    Serial.println(WiFi.localIP());
  }
  else
  {
    // --- Em modo de configuração ---
    Serial.println("Iniciado em modo AP para configuração.");
  }
}

void loop()
{
  provisioner.loop();

  if (provisioner.isConnected())
  {
    dashboardServer.loop(); // Processa clientes web
    checkLightSchedule();   // Sua lógica de controle

    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL)
    {
      printSerialStatus();
      lastSerialPrint = millis();
    }
  }

  delay(10);
}