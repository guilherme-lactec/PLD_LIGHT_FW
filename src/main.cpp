#include <Arduino.h>
#include "WiFiProvisioner.h"
#include "time.h" // Para NTP

// --- Configuração da Biblioteca ---
// Você pode mudar o nome do hotspot aqui se quiser
WiFiProvisioner provisioner("ESP32-Config");

// --- Configuração do NTP ---
// Servidor NTP brasileiro
const char *ntpServer = "a.st1.ntp.br";
// Fuso de Curitiba (GMT-3), sem horário de verão
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

// --- Variáveis de Controle ---
bool ntpInitialized = false;
unsigned long lastTimePrint = 0;

/**
 * @brief Inicia a sincronização com o servidor NTP.
 */
void initNTP()
{
  Serial.println("Configurando NTP...");
  // Inicia o cliente NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora. Isso pode levar alguns segundos...");
}

/**
 * @brief Tenta obter a hora local e a imprime no Serial.
 */
void printLocalTime()
{
  struct tm timeinfo;

  // Tenta obter a hora
  if (!getLocalTime(&timeinfo))
  {
    if (!ntpInitialized)
    {
      Serial.println("...aguardando sincronia NTP");
    }
    return;
  }

  // Se chegou aqui, a hora foi obtida
  if (!ntpInitialized)
  {
    Serial.println("--- HORA SINCRONIZADA ---");
    ntpInitialized = true; // Marca como inicializado
  }

  // Formata e imprime a data/hora
  // %A = Dia da semana, %d = dia, %B = Mês, %Y = Ano
  // %H = Hora, %M = Minuto, %S = Segundo
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%A, %d de %B de %Y", &timeinfo);
  Serial.printf("Data: %s\n", buffer);
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  Serial.printf("Hora: %s\n", buffer);
  Serial.println("---------------------------------");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nIniciando...");

  // Tenta conectar. Se retornar 'true', estamos conectados.
  if (provisioner.begin())
  {
    // Conectado ao WiFi com sucesso
    initNTP(); // Inicia a busca pela hora
  }
  else
  {
    // Não conectado, está em modo AP
    Serial.println("Iniciado em modo AP para configuração.");
    Serial.println("Conecte-se à rede 'ESP32-Config'.");
  }
}

void loop()
{
  // A biblioteca *precisa* ser executada no loop
  provisioner.loop();

  // Só executa o resto se estivermos conectados
  if (provisioner.isConnected())
  {

    // Imprime a hora a cada 10 segundos
    if (millis() - lastTimePrint > 10000)
    {
      printLocalTime();
      lastTimePrint = millis();
    }
  }

  delay(10); // Pequeno delay para estabilidade
}