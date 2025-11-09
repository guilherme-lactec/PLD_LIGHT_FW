#include <Arduino.h>
#include "WiFiProvisioner.h"
#include "DashboardServer.h"
#include "time.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_Sensor.h> // <-- NOVO: Biblioteca DHT
#include <DHT.h>             // <-- NOVO: Biblioteca DHT

// TODO
//  -> add grafhs for the month
//  -> fix update rate ( too fast cant change parameters, to slow cant see time change)
//  -> fix pwm -> 1% no qual 0.12V

// --- Configuração das Bibliotecas ---
WiFiProvisioner provisioner("ESP32-Config");
DashboardServer dashboardServer(80);
Preferences preferences;

// --- Configuração do NTP ---
const char *ntpServer = "a.st1.ntp.br";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

// --- Configuração do PWM (LEDC) ---
const int LEDC_PIN = 27;
const int LEDC_CHANNEL = 0;
const int LEDC_FREQ = 5000;
const int LEDC_RESOLUTION = 8;
const int RAMP_DURATION_MINUTES = 60;

// --- Configuração dos Sensores ---
#define DHTPIN 25
#define DHTTYPE DHT11     // Mude para DHT11 se for o seu sensor
#define LDR_PIN 35        // Pino do sensor de luminosidade
DHT dht(DHTPIN, DHTTYPE); // Objeto do sensor DHT
// --- Variáveis de Controle ---
bool ntpInitialized = false;
unsigned long lastSerialPrint = 0;
unsigned long lastPwmUpdate = 0;
unsigned long lastSensorRead = 0; // <-- NOVO: Timer para sensores
const unsigned long SERIAL_PRINT_INTERVAL = 10000;
const unsigned long SENSOR_READ_INTERVAL = 5000; // Ler sensores a cada 5s
int currentPwm = 0;

// =========================================================
// --- DADOS DO SEU PROJETO (SENSORES E ESTADO) ---
// Sensores (agora com valores reais)
float currentTemperature = 0.0;
float currentHumidity = 0.0;
int currentLuminosity = 0; // Valor 0-4095
// Configurações
String horaLigar;
String horaDesligar;
int luzMaximaSalva;
// =========================================================

/**
 * @brief Converte "HH:MM" para minutos.
 */
int parseTimeMinutes(String hh_mm)
{
  if (hh_mm.length() != 5)
    return 0;
  int hour = hh_mm.substring(0, 2).toInt();
  int minute = hh_mm.substring(3, 5).toInt();
  return (hour * 60) + minute;
}

/**
 * @brief Lógica da rampa de luz.
 */
void updateLightPwm()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return;
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int ligarMinutes = parseTimeMinutes(horaLigar);
  int desligarMinutes = parseTimeMinutes(horaDesligar);
  int rampStartMinutes = ligarMinutes - RAMP_DURATION_MINUTES;
  int fadeStartMinutes = desligarMinutes - RAMP_DURATION_MINUTES;
  float maxPwmFloat = (float)luzMaximaSalva * 2.55;
  int maxPwm = (int)min(maxPwmFloat, 255.0f);
  int newPwm = 0;
  bool overnight = (desligarMinutes < ligarMinutes);

  if (overnight)
  {
    if (currentMinutes >= rampStartMinutes && currentMinutes < ligarMinutes)
    {
      newPwm = map(currentMinutes, rampStartMinutes, ligarMinutes, 0, maxPwm);
    }
    else if (currentMinutes >= fadeStartMinutes && currentMinutes < desligarMinutes)
    {
      newPwm = map(currentMinutes, fadeStartMinutes, desligarMinutes, maxPwm, 0);
    }
    else if (currentMinutes >= ligarMinutes || currentMinutes < fadeStartMinutes)
    {
      newPwm = maxPwm;
    }
    else
    {
      newPwm = 0;
    }
  }
  else
  {
    if (currentMinutes >= rampStartMinutes && currentMinutes < ligarMinutes)
    {
      newPwm = map(currentMinutes, rampStartMinutes, ligarMinutes, 0, maxPwm);
    }
    else if (currentMinutes >= fadeStartMinutes && currentMinutes < desligarMinutes)
    {
      newPwm = map(currentMinutes, fadeStartMinutes, desligarMinutes, maxPwm, 0);
    }
    else if (currentMinutes >= ligarMinutes && currentMinutes < fadeStartMinutes)
    {
      newPwm = maxPwm;
    }
    else
    {
      newPwm = 0;
    }
  }

  if (newPwm != currentPwm)
  {
    currentPwm = newPwm;
    ledcWrite(LEDC_CHANNEL, currentPwm);
  }
}

/**
 * @brief NOVO: Função para ler os sensores de hardware.
 * É chamada pelo timer no loop().
 */
void atualizarSensoresReais()
{
  // 1. Leitura do DHT22
  // A leitura pode falhar. Se falhar (isNaN), mantém o último valor bom.
  float newTemp = dht.readTemperature();
  if (!isnan(newTemp))
  {
    currentTemperature = newTemp;
  }
  else
  {
    Serial.println("[Sensor] Falha ao ler temperatura do DHT!");
  }

  float newHum = dht.readHumidity();
  if (!isnan(newHum))
  {
    currentHumidity = newHum;
  }
  else
  {
    Serial.println("[Sensor] Falha ao ler humidade do DHT!");
  }

  // 2. Leitura do Sensor de Luminosidade (LDR)
  // O ADC de 12 bits do ESP32 retorna valores de 0 (0V) a 4095 (3.3V)
  currentLuminosity = analogRead(LDR_PIN);

  // Descomente para debug
  // Serial.printf("[Sensor] T:%.1f C, H:%.1f %%, L:%d\n", currentTemperature, currentHumidity, currentLuminosity);
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
  Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str()); // Corrigido de .c.str()

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

  // ATUALIZADO: Mostra os valores reais (raw para LDR)
  Serial.printf("  Sensores: Temp=%.1f C, Hum=%.1f %%, Lum=%d (raw)\n",
                currentTemperature, currentHumidity, currentLuminosity);
  Serial.printf("  Config: Luz Ligar=%s, Desligar=%s, Max=%d%% (PWM: %d/255)\n",
                horaLigar.c_str(), horaDesligar.c_str(), luzMaximaSalva, currentPwm);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nIniciando...");

  // *** NVS ***
  preferences.begin("app-settings", false);
  horaLigar = preferences.getString("horaLigar", "08:00");
  horaDesligar = preferences.getString("horaDesligar", "18:00");
  luzMaximaSalva = preferences.getInt("luzMaxima", 80);
  preferences.end();
  Serial.println("Configurações carregadas da NVS.");

  // *** INICIALIZAÇÃO DOS SENSORES REAIS ***
  Serial.println("Iniciando sensores...");
  dht.begin(); // Inicializa o DHT
  // O analogRead (pino 34) não precisa de pinMode, mas é bom configurar
  pinMode(LDR_PIN, INPUT);

  // *** PWM (LEDC) ***
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, LEDC_RESOLUTION);
  ledcAttachPin(LEDC_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, 0);
  currentPwm = 0;

  if (provisioner.begin())
  {
    // --- Conectado com sucesso ---
    initNTP();

    // CALLBACK 1: O que o ESP32 ENVIA para a web (GET)
    dashboardServer.onDataRequest([](JsonDocument &doc)
                                  {
            
            // NÃO lemos sensores aqui. Apenas reportamos os valores
            // que foram lidos pelo timer no loop() principal.
            
            doc["temperatura"] = currentTemperature;
            doc["humidade"] = currentHumidity;
            doc["luminosidade"] = currentLuminosity; // Envia o valor 0-4095
            
            doc["hora_ligar"] = horaLigar;       
            doc["hora_desligar"] = horaDesligar; 
            doc["luz_maxima"] = luzMaximaSalva; });

    // CALLBACK 2: O que o ESP32 RECEBE da web (POST)
    dashboardServer.onSettingsRequest([](String ligar, String desligar, int luzMaxima)
                                      {
            
            horaLigar = ligar;
            horaDesligar = desligar;
            luzMaximaSalva = luzMaxima;

            preferences.begin("app-settings", false); 
            preferences.putString("horaLigar", horaLigar);
            preferences.putString("horaDesligar", horaDesligar);
            preferences.putInt("luzMaxima", luzMaximaSalva);
            preferences.end(); 

            Serial.println("\n!!! NOVAS CONFIGURAÇÕES SALVAS NA NVS !!!");
            Serial.printf("Ligar às: %s\n", horaLigar.c_str());
            Serial.printf("Desligar às: %s\n", horaDesligar.c_str());
            Serial.printf("Luz Máxima: %d%%\n\n", luzMaximaSalva); });

    dashboardServer.begin();
    Serial.print("Acesse o dashboard em: http://");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Iniciado em modo AP para configuração.");
  }
}

void loop()
{
  provisioner.loop();

  if (provisioner.isConnected())
  {
    dashboardServer.loop(); // Processa clientes web

    // --- LÓGICA DA LUZ ---
    if (ntpInitialized && (millis() - lastPwmUpdate > 1000))
    {
      lastPwmUpdate = millis();
      updateLightPwm(); // Chama a função de lógica da rampa
    }

    // --- LÓGICA DE LEITURA DE SENSORES ---
    // Lê os sensores de hardware em um intervalo separado
    if (millis() - lastSensorRead > SENSOR_READ_INTERVAL)
    {
      lastSensorRead = millis();
      atualizarSensoresReais(); // Chama a nova função de leitura
    }

    // --- LÓGICA DE IMPRESSÃO SERIAL ---
    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL)
    {
      printSerialStatus();
      lastSerialPrint = millis();
    }
  }

  delay(10); // Pequeno delay para estabilidade
}