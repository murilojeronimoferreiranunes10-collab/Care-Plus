// ============================================================
//  CARE PLUS — ESP32 + DHT22 + OLED + FIWARE MQTT
//  Integrado com IoT Agent UltraLight 2.0
// ============================================================

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// CONFIGURAÇÕES
// ============================================================

const char* SSID        = "Wokwi-GUEST";
const char* PASSWORD    = "";

const char* BROKER_MQTT = "34.135.50.230";
const int   BROKER_PORT = 1883;

// ============================================================
// DEVICE
// ============================================================

const char* DEVICE_ID = "lamp003";

// ============================================================
// TÓPICOS MQTT
// ============================================================

const char* TOPICO_SUBSCRIBE       = "/TEF/lamp003/cmd";
const char* TOPICO_PUBLISH_STATUS  = "/TEF/lamp003/attrs";
const char* TOPICO_PUBLISH_BPM     = "/TEF/lamp003/attrs/bpm";
const char* TOPICO_PUBLISH_SPO2    = "/TEF/lamp003/attrs/spo2";
const char* TOPICO_PUBLISH_SENSOR  = "/TEF/lamp003/attrs/sensor";

const char* ID_MQTT = "fiware_001";

// ============================================================
// PINOS
// ============================================================

#define LED_PIN   2
#define DHTPIN    15
#define DHTTYPE   DHT22

#define SDA_PIN   21
#define SCL_PIN   22

// ============================================================
// DISPLAY OLED
// ============================================================

#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C

// ============================================================
// OBJETOS
// ============================================================

WiFiClient espClient;
PubSubClient MQTT(espClient);

DHT dht(DHTPIN, DHTTYPE);

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ============================================================
// ESTADO GLOBAL
// ============================================================

bool ledLigado = false;

float temperatura = 0;
float umidade     = 0;

int BPM  = 0;
int SpO2 = 0;

uint32_t ultimoEnvio = 0;
const unsigned long INTERVALO_MS = 3000;

// ============================================================
// ENUM
// ============================================================

enum EstadoSaude {
  SAUDAVEL,
  ATENCAO,
  ALERTA,
  CRITICO
};

EstadoSaude estadoFinal = SAUDAVEL;

String causaAlerta = "OK";

// ============================================================
// PROTÓTIPOS
// ============================================================

void initWiFi();
void reconnectMQTT();
void verificaConexoes();

void mqttCallback(char* topic, byte* payload, unsigned int length);

bool lerSensores();

EstadoSaude classificaBPM(int bpm);
EstadoSaude classificaSpO2(int spo2);

EstadoSaude maiorEstado(EstadoSaude a, EstadoSaude b);

void detectarCausa(EstadoSaude eBPM, EstadoSaude eSpO2);

void atualizarDisplay();
void publicarMQTT();
void printSerial();

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("       CARE PLUS — Iniciando...         ");
  Serial.println("========================================");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);

  dht.begin();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {

    Serial.println("[ERRO] OLED não encontrado!");

  } else {

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);

    display.setCursor(10, 10);
    display.println("CARE");

    display.setCursor(10, 35);
    display.println("PLUS");

    display.display();

    delay(1500);
  }

  initWiFi();

  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqttCallback);

  reconnectMQTT();

  MQTT.publish(TOPICO_PUBLISH_STATUS, "s|off");

  Serial.println("[MQTT] Estado inicial publicado");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  verificaConexoes();

  MQTT.loop();

  if (millis() - ultimoEnvio >= INTERVALO_MS) {

    ultimoEnvio = millis();

    if (!lerSensores()) {
      return;
    }

    EstadoSaude eBPM  = classificaBPM(BPM);
    EstadoSaude eSpO2 = classificaSpO2(SpO2);

    estadoFinal = maiorEstado(eBPM, eSpO2);

    detectarCausa(eBPM, eSpO2);

    printSerial();

    atualizarDisplay();

    publicarMQTT();
  }
}

// ============================================================
// WIFI
// ============================================================

void initWiFi() {

  Serial.print("[WiFi] Conectando");

  WiFi.begin(SSID, PASSWORD);

  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

    tentativas++;

    if (tentativas > 40) {

      Serial.println("\n[WiFi] Timeout!");

      ESP.restart();
    }
  }

  Serial.println();
  Serial.println("[WiFi] Conectado!");

  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

// ============================================================
// CALLBACK MQTT
// ============================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String msg = "";

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("[MQTT] Mensagem: ");
  Serial.println(msg);

  String cmdOn  = String(DEVICE_ID) + "@on|";
  String cmdOff = String(DEVICE_ID) + "@off|";

  if (msg == cmdOn) {

    ledLigado = true;

    digitalWrite(LED_PIN, HIGH);

    MQTT.publish(TOPICO_PUBLISH_STATUS, "s|on");

    Serial.println("[CMD] LED ON");
  }

  else if (msg == cmdOff) {

    ledLigado = false;

    digitalWrite(LED_PIN, LOW);

    MQTT.publish(TOPICO_PUBLISH_STATUS, "s|off");

    Serial.println("[CMD] LED OFF");
  }
}

// ============================================================
// RECONECTA MQTT
// ============================================================

void reconnectMQTT() {

  while (!MQTT.connected()) {

    Serial.print("[MQTT] Conectando...");

    if (MQTT.connect(ID_MQTT)) {

      Serial.println(" conectado!");

      MQTT.subscribe(TOPICO_SUBSCRIBE);

      Serial.print("[MQTT] Inscrito em: ");
      Serial.println(TOPICO_SUBSCRIBE);

    } else {

      Serial.print(" erro: ");
      Serial.println(MQTT.state());

      delay(2000);
    }
  }
}

// ============================================================
// VERIFICA CONEXÕES
// ============================================================

void verificaConexoes() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("[WiFi] Reconectando...");
    initWiFi();
  }

  if (!MQTT.connected()) {

    Serial.println("[MQTT] Reconectando...");
    reconnectMQTT();
  }
}

// ============================================================
// LEITURA DOS SENSORES
// ============================================================

bool lerSensores() {

  float t = dht.readTemperature();
  float u = dht.readHumidity();

  if (isnan(t) || isnan(u)) {

    Serial.println("[DHT22] Erro leitura");

    return false;
  }

  temperatura = t;
  umidade     = u;

  // Simulação BPM e SpO2

  BPM = map((long)(temperatura * 10), 150, 450, 60, 140);

  SpO2 = map((long)(umidade * 10), 200, 1000, 85, 100);

  BPM  = constrain(BPM, 30, 220);
  SpO2 = constrain(SpO2, 80, 100);

  return true;
}

// ============================================================
// CLASSIFICAÇÃO
// ============================================================

EstadoSaude classificaBPM(int bpm) {

  if (bpm >= 60 && bpm <= 100)
    return SAUDAVEL;

  if ((bpm >= 50 && bpm < 60) || (bpm > 100 && bpm <= 120))
    return ATENCAO;

  if ((bpm >= 40 && bpm < 50) || (bpm > 120 && bpm <= 150))
    return ALERTA;

  return CRITICO;
}

EstadoSaude classificaSpO2(int spo2) {

  if (spo2 >= 95)
    return SAUDAVEL;

  if (spo2 >= 90)
    return ATENCAO;

  if (spo2 >= 85)
    return ALERTA;

  return CRITICO;
}

EstadoSaude maiorEstado(EstadoSaude a, EstadoSaude b) {

  return (a > b) ? a : b;
}

void detectarCausa(EstadoSaude eBPM, EstadoSaude eSpO2) {

  if (eBPM >= ALERTA && eSpO2 >= ALERTA) {
    causaAlerta = "BPM+SPO2";
  }

  else if (eBPM >= ALERTA) {
    causaAlerta = "BPM";
  }

  else if (eSpO2 >= ALERTA) {
    causaAlerta = "SPO2";
  }

  else {
    causaAlerta = "OK";
  }
}

// ============================================================
// SERIAL
// ============================================================

void printSerial() {

  const char* estados[] = {
    "SAUDAVEL",
    "ATENCAO",
    "ALERTA",
    "CRITICO"
  };

  Serial.println("--------------------------------");

  Serial.print("Temp: ");
  Serial.println(temperatura);

  Serial.print("Umid: ");
  Serial.println(umidade);

  Serial.print("BPM: ");
  Serial.println(BPM);

  Serial.print("SpO2: ");
  Serial.println(SpO2);

  Serial.print("Estado: ");
  Serial.println(estados[estadoFinal]);

  Serial.print("Alerta: ");
  Serial.println(causaAlerta);

  Serial.println("--------------------------------");
}

// ============================================================
// OLED
// ============================================================

void atualizarDisplay() {

  const char* estados[] = {
    "SAUDAVEL",
    "ATENCAO",
    "ALERTA",
    "CRITICO"
  };

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("== CARE PLUS ==");

  display.setTextSize(2);

  display.setCursor(0, 15);
  display.print("B:");
  display.println(BPM);

  display.setCursor(0, 40);
  display.print("S:");
  display.print(SpO2);
  display.println("%");

  display.setTextSize(1);

  display.setCursor(70, 54);
  display.println(estados[estadoFinal]);

  display.display();
}

// ============================================================
// PUBLICAR MQTT
// ============================================================

void publicarMQTT() {

  // UltraLight

  String ul = "";

  ul += "s|";
  ul += ledLigado ? "on" : "off";

  ul += "|bpm|";
  ul += String(BPM);

  ul += "|spo2|";
  ul += String(SpO2);

  ul += "|temp|";
  ul += String(temperatura, 1);

  ul += "|umid|";
  ul += String(umidade, 1);

  MQTT.publish(TOPICO_PUBLISH_STATUS, ul.c_str());

  Serial.print("[MQTT] UL -> ");
  Serial.println(ul);

  // JSON

  String json = "{";

  json += "\"bpm\":";
  json += String(BPM);
  json += ",";

  json += "\"spo2\":";
  json += String(SpO2);
  json += ",";

  json += "\"temperatura\":";
  json += String(temperatura, 1);
  json += ",";

  json += "\"umidade\":";
  json += String(umidade, 1);
  json += ",";

  json += "\"estado\":";
  json += String((int)estadoFinal);
  json += ",";

  json += "\"alerta\":\"";
  json += causaAlerta;
  json += "\"";

  json += "}";

  MQTT.publish(TOPICO_PUBLISH_SENSOR, json.c_str());

  Serial.print("[MQTT] JSON -> ");
  Serial.println(json);

  // tópicos individuais

  MQTT.publish(
    TOPICO_PUBLISH_BPM,
    String(BPM).c_str(),
    true
  );

  MQTT.publish(
    TOPICO_PUBLISH_SPO2,
    String(SpO2).c_str(),
    true
  );
}