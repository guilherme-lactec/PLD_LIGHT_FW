#include <Arduino.h>
#include "WiFiProvisioner.h"
#include "DashboardServer.h"
#include "time.h"
#include <ArduinoJson.h>
#include <Preferences.h>

// --- Configuração das Bibliotecas ---
WiFiProvisioner provisioner("ESP32-Config");
DashboardServer dashboardServer(80);
Preferences preferences;

// --- Configuração do NTP ---
const char *ntpServer = "a.st1.ntp.br";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

// --- Configuração do PWM (LEDC) ---
const int LEDC_PIN = 25;
const int LEDC_CHANNEL = 0;
const int LEDC_FREQ = 5000;           // 5 kHz
const int LEDC_RESOLUTION = 8;        // 8 bits (0-255)
const int RAMP_DURATION_MINUTES = 60; // 1 hora de rampa

// --- Variáveis de Controle ---
bool ntpInitialized = false;
unsigned long lastSerialPrint = 0;
unsigned long lastPwmUpdate = 0; // Timer para a lógica da luz
const unsigned long SERIAL_PRINT_INTERVAL = 10000;
int currentPwm = 0; // Armazena o valor PWM atual

// =========================================================
// --- DADOS DO SEU PROJETO (SENSORES E ESTADO) ---
// Sensores
float tempFicticia = 25.0;
float humFicticia = 60.0;
int lumFicticia = 800;
// Configurações (com valores padrão)
String horaLigar;
String horaDesligar;
int luzMaximaSalva; // ATUALIZADO: Renomeado
// =========================================================

/**
 * @brief Converte uma string "HH:MM" para total de minutos desde a meia-noite.
 */
int parseTimeMinutes(String hh_mm)
{
  if (hh_mm.length() != 5)
    return 0; // Proteção
  int hour = hh_mm.substring(0, 2).toInt();
  int minute = hh_mm.substring(3, 5).toInt();
  return (hour * 60) + minute;
}

/**
 * @brief Função principal da lógica de luz.
 * Calcula e define o PWM com base na hora atual e nas configurações.
 */
void updateLightPwm()
{
  struct tm timeinfo;
  // Se não conseguir obter a hora, não faz nada
  if (!getLocalTime(&timeinfo))
  {
    return;
  }

  // 1. Obter todos os tempos em minutos
  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int ligarMinutes = parseTimeMinutes(horaLigar);
  int desligarMinutes = parseTimeMinutes(horaDesligar);

  // 2. Calcular os pontos de início e fim da rampa
  int rampStartMinutes = ligarMinutes - RAMP_DURATION_MINUTES;    // 1h antes de ligar
  int fadeStartMinutes = desligarMinutes - RAMP_DURATION_MINUTES; // 1h antes de desligar

  // 3. Calcular o valor máximo de PWM (0-255) com base na % (0-100)
  // Usamos 'float' para precisão e 'min()' para evitar estouro
  float maxPwmFloat = (float)luzMaximaSalva * 2.55; // (luzMaxima / 100) * 255
  int maxPwm = (int)min(maxPwmFloat, 255.0f);

  int newPwm = 0; // O valor padrão é desligado

  // 4. Lógica de estado (trata "overnight" ex: ligar 18:00, desligar 06:00)
  bool overnight = (desligarMinutes < ligarMinutes);

  if (overnight)
  {
    // --- Período Noturno (Ex: 18:00 - 06:00) ---
    if (currentMinutes >= rampStartMinutes && currentMinutes < ligarMinutes)
    {
      // Rampa de subida (ex: 17:00 - 18:00)
      newPwm = map(currentMinutes, rampStartMinutes, ligarMinutes, 0, maxPwm);
    }
    else if (currentMinutes >= fadeStartMinutes && currentMinutes < desligarMinutes)
    {
      // Rampa de descida (ex: 05:00 - 06:00)
      newPwm = map(currentMinutes, fadeStartMinutes, desligarMinutes, maxPwm, 0);
    }
    else if (currentMinutes >= ligarMinutes || currentMinutes < fadeStartMinutes)
    {
      // Luz Acesa (ex: 18:00 - 23:59 OU 00:00 - 05:00)
      newPwm = maxPwm;
    }
    else
    {
      // Luz Apagada (ex: 06:00 - 17:00)
      newPwm = 0;
    }
  }
  else
  {
    // --- Período Diurno (Ex: 08:00 - 18:00) ---
    if (currentMinutes >= rampStartMinutes && currentMinutes < ligarMinutes)
    {
      // Rampa de subida (ex: 07:00 - 08:00)
      newPwm = map(currentMinutes, rampStartMinutes, ligarMinutes, 0, maxPwm);
    }
    else if (currentMinutes >= fadeStartMinutes && currentMinutes < desligarMinutes)
    {
      // Rampa de descida (ex: 17:00 - 18:00)
      newPwm = map(currentMinutes, fadeStartMinutes, desligarMinutes, maxPwm, 0);
    }
    else if (currentMinutes >= ligarMinutes && currentMinutes < fadeStartMinutes)
    {
      // Luz Acesa (ex: 08:00 - 17:00)
      newPwm = maxPwm;
    }
    else
    {
      // Luz Apagada (fora do período)
      newPwm = 0;
    }
  }

  // 5. Aplicar o PWM (apenas se o valor mudou)
  if (newPwm != currentPwm)
  {
    currentPwm = newPwm;
    ledcWrite(LEDC_CHANNEL, currentPwm);

    // Descomente para debug pesado
    // Serial.printf("[Light] Minutos: %d, Novo PWM: %d\n", currentMinutes, currentPwm);
  }
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
  // ATUALIZADO:
  Serial.printf("  Config: Luz Ligar=%s, Desligar=%s, Max=%d%% (PWM: %d/255)\n",
                horaLigar.c_str(), horaDesligar.c_str(), luzMaximaSalva, currentPwm);
}

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

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nIniciando...");

  // *** NVS ***
  // Carrega as configurações salvas
  preferences.begin("app-settings", false);

  // ATUALIZADO: Carrega as configs com os nomes novos/antigos
  horaLigar = preferences.getString("horaLigar", "08:00");
  horaDesligar = preferences.getString("horaDesligar", "18:00");
  // Usamos a nova chave, mas um valor padrão de 80
  luzMaximaSalva = preferences.getInt("luzMaxima", 80);

  preferences.end();

  Serial.println("Configurações carregadas da NVS:");
  Serial.printf("  Ligar: %s, Desligar: %s, Max: %d%%\n", horaLigar.c_str(), horaDesligar.c_str(), luzMaximaSalva);

  // *** PWM (LEDC) ***
  // Configura o pino 25
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcAttachPin(LEDC_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, 0); // Garante que a luz começa desligada
  currentPwm = 0;

  if (provisioner.begin())
  {
    // --- Conectado com sucesso ---
    initNTP();

    // CALLBACK 1: O que o ESP32 ENVIA para a web (GET)
    dashboardServer.onDataRequest([](JsonDocument &doc)
                                  {
                                    atualizarSensores(); // Atualiza leituras

                                    doc["temperatura"] = tempFicticia;
                                    doc["humidade"] = humFicticia;
                                    doc["luminosidade"] = lumFicticia;

                                    // ATUALIZADO: Envia as chaves corretas
                                    doc["hora_ligar"] = horaLigar;
                                    doc["hora_desligar"] = horaDesligar;
                                    doc["luz_maxima"] = luzMaximaSalva; // Key atualizada
                                  });

    // CALLBACK 2: O que o ESP32 RECEBE da web (POST)
    // ATUALIZADO: A assinatura do lambda mudou
    dashboardServer.onSettingsRequest([](String ligar, String desligar, int luzMaxima)
                                      {
                                        // 1. Salva nas variáveis globais (RAM)
                                        horaLigar = ligar;
                                        horaDesligar = desligar;
                                        luzMaximaSalva = luzMaxima; // Nome atualizado

                                        // 2. Salva na memória não volátil (NVS)
                                        preferences.begin("app-settings", false);
                                        preferences.putString("horaLigar", horaLigar);
                                        preferences.putString("horaDesligar", horaDesligar);
                                        preferences.putInt("luzMaxima", luzMaximaSalva); // Chave atualizada
                                        preferences.end();

                                        // Imprime no Serial para confirmar
                                        Serial.println("\n!!! NOVAS CONFIGURAÇÕES SALVAS NA NVS !!!");
                                        Serial.printf("Ligar às: %s\n", horaLigar.c_str());
                                        Serial.printf("Desligar às: %s\n", horaDesligar.c_str());
                                        Serial.printf("Luz Máxima: %d%%\n\n", luzMaximaSalva); // Msg atualizada
                                      });

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

    // *** LÓGICA DA LUZ ***
    // Executa a verificação da luz a cada 1 segundo,
    // mas APENAS se o NTP já estiver sincronizado.
    if (ntpInitialized && (millis() - lastPwmUpdate > 1000))
    {
      lastPwmUpdate = millis();
      updateLightPwm(); // Chama a nova função de lógica
    }

    // Imprime o status no Serial a cada 10 segundos
    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL)
    {
      printSerialStatus();
      lastSerialPrint = millis();
    }
  }

  delay(10);
}