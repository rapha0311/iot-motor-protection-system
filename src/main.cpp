#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ======================================================
// CONFIGURE SEUS DADOS DO ADAFRUIT IO AQUI
// ======================================================
#define IO_USERNAME  "RaphaIoT"
#define IO_KEY       "SUA_CHAVE_AQUI"

// Pinos e Sensores
#define PIN_DS18B20 4
#define PIN_RELE 5

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
Adafruit_MPU6050 mpu;

// Configurações de Rede (Wokwi GUEST) e Broker Adafruit IO
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW); // Estado normal: Contator ligado

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Conectado!");

  // Inicialização dos Sensores
  sensors.begin();
  Wire.begin(21, 22);
  if (!mpu.begin()) {
    Serial.println("Erro ao encontrar o MPU6050!");
  }

  client.setServer(mqtt_server, mqtt_port);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao Adafruit IO...");
    // Autenticação usando Username e Key do Adafruit
    if (client.connect("ESP32_Motor_Client", IO_USERNAME, IO_KEY)) {
      Serial.println("Conectado com sucesso!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s...");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // 1. Leitura de Temperatura
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // 2. Leitura de Vibração
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float vibracaoTotal = sqrt(a.acceleration.x * a.acceleration.x + 
                             a.acceleration.y * a.acceleration.y + 
                             a.acceleration.z * a.acceleration.z);

  // 3. Lógica de Proteção
  bool falha = false;
  if (tempC > 70.0 || vibracaoTotal > 15.0) {
    digitalWrite(PIN_RELE, HIGH); // Desarma contator
    falha = true;
    Serial.println("ALERTA: Motor Desarmado!");
  } else {
    digitalWrite(PIN_RELE, LOW);  // Operação Normal
  }

  // 4. Publicação nos Feeds do Adafruit IO
  char topicTemp[64], topicVib[64], topicFalha[64];
  
  snprintf(topicTemp, sizeof(topicTemp), "%s/feeds/temperatura", IO_USERNAME);
  snprintf(topicVib, sizeof(topicVib), "%s/feeds/vibracao", IO_USERNAME);
  snprintf(topicFalha, sizeof(topicFalha), "%s/feeds/status-falha", IO_USERNAME);

  client.publish(topicTemp, String(tempC).c_str());
  client.publish(topicVib, String(vibracaoTotal).c_str());
  client.publish(topicFalha, falha ? "1" : "0");

  delay(3000); // Envio a cada 3 segundos
}
