# 🫀 Care Plus — Monitoramento de Saúde com ESP32 + FIWARE

> Sistema de monitoramento de sinais vitais em tempo real utilizando ESP32, sensor DHT22, protocolo MQTT e stack FIWARE na nuvem.

---
## Integrantes
Murilo Jeronimo Ferreira Nunes RM560641
Bruno Santos Castilho RM566799
Vinicius Kozonoe Guaglini RM567264

## 📋 Sumário

- [Visão Geral](#visão-geral)
- [Arquitetura da Solução](#arquitetura-da-solução)
- [Hardware Utilizado](#hardware-utilizado)
- [Tecnologias e Protocolos](#tecnologias-e-protocolos)
- [Classificação de Saúde](#classificação-de-saúde)
- [Tópicos MQTT](#tópicos-mqtt)
- [Configuração FIWARE](#configuração-fiware)
- [Como Executar](#como-executar)
- [Sequência de Deploy](#sequência-de-deploy)
- [Endpoints da API](#endpoints-da-api)
- [Estrutura do Projeto](#estrutura-do-projeto)

---

## Visão Geral

O **Care Plus** é um sistema IoT voltado para monitoramento de saúde em ambientes hospitalares ou domiciliares. Um dispositivo **ESP32** coleta dados de temperatura e umidade via sensor **DHT22** e, a partir dessas leituras, simula métricas vitais como **BPM (batimentos cardíacos)** e **SpO2 (saturação de oxigênio)**.

Os dados são publicados via **MQTT** em tópicos específicos e processados pela stack **FIWARE** (IoT Agent + Orion Context Broker), tornando as informações disponíveis via **API REST NGSIv2** para qualquer aplicação.

O dispositivo também classifica automaticamente o estado de saúde em **4 níveis** e exibe os dados em tempo real num **display OLED 128x64**, além de aceitar comandos remotos para ligar/desligar um LED indicador.

---

## Arquitetura da Solução

O sistema é organizado em **4 camadas**:

```
┌─────────────────────────────────────────────────────────┐
│                    CAMADA EDGE                          │
│   DHT22 ──► ESP32 (classifica) ──► OLED / LED GPIO 2   │
└──────────────────────┬──────────────────────────────────┘
                       │ UltraLight 2.0  ▲ cmd on/off
                       ▼                 │
┌─────────────────────────────────────────────────────────┐
│                    CAMADA MQTT                          │
│   Broker Mosquitto · /TEF/lamp003/attrs · /cmd          │
└──────────────────────┬──────────────────────────────────┘
                       │ Northbound · port 4041
                       ▼
┌─────────────────────────────────────────────────────────┐
│               CAMADA BACK-END (FIWARE)                  │
│   IoT Agent MQTT ──► Orion Context Broker ──► MongoDB   │
└──────────────────────┬──────────────────────────────────┘
                       │ GET/PATCH NGSIv2 · port 1026
                       ▼
┌─────────────────────────────────────────────────────────┐
│                  CAMADA APPLICATION                     │
│      Dashboard · Alertas · Prontuário / Integração      │
└─────────────────────────────────────────────────────────┘
```

> O arquivo `CarePlus_Arquitetura.drawio` contém o diagrama completo editável. Abra com [draw.io](https://app.diagrams.net/).

---

## Hardware Utilizado

| Componente | Função |
|---|---|
| **ESP32** | Microcontrolador principal |
| **DHT22** | Sensor de temperatura e umidade (GPIO 15) |
| **OLED SSD1306 128x64** | Display local de sinais vitais (I2C: SDA 21 / SCL 22) |
| **LED** | Indicador de status remoto (GPIO 2) |

---

## Tecnologias e Protocolos

| Camada | Tecnologia |
|---|---|
| Firmware | C++ (Arduino / ESP-IDF) |
| Conectividade | Wi-Fi 2.4 GHz |
| Protocolo IoT | MQTT (PubSubClient) |
| Payload | **UltraLight 2.0** |
| Broker | Mosquitto · `34.135.50.230:1883` |
| IoT Agent | FIWARE IoT Agent MQTT · porta `4041` |
| Context Broker | FIWARE Orion · porta `1026` |
| API | **NGSIv2 REST** |
| Banco de Dados | MongoDB |
| Simulador | Wokwi (Wi-Fi SSID: `Wokwi-GUEST`) |

---

## Classificação de Saúde

O firmware classifica automaticamente o estado do paciente com base nas leituras simuladas:

### BPM

| Estado | Faixa |
|---|---|
| ✅ Saudável | 60 – 100 bpm |
| ⚠️ Atenção | 50–59 ou 101–120 bpm |
| 🟠 Alerta | 40–49 ou 121–150 bpm |
| 🔴 Crítico | < 40 ou > 150 bpm |

### SpO2

| Estado | Faixa |
|---|---|
| ✅ Saudável | ≥ 95% |
| ⚠️ Atenção | 90 – 94% |
| 🟠 Alerta | 85 – 89% |
| 🔴 Crítico | < 85% |

O estado final publicado é sempre o **pior** entre BPM e SpO2. O campo `alerta` indica a causa: `OK`, `BPM`, `SPO2` ou `BPM+SPO2`.

---

## Tópicos MQTT

| Direção | Tópico | Conteúdo |
|---|---|---|
| Publish | `/TEF/lamp003/attrs` | Payload UltraLight completo |
| Publish | `/TEF/lamp003/attrs/bpm` | BPM (retained) |
| Publish | `/TEF/lamp003/attrs/spo2` | SpO2 (retained) |
| Publish | `/TEF/lamp003/attrs/sensor` | JSON com todos os campos |
| Subscribe | `/TEF/lamp003/cmd` | Comandos `on` / `off` |

### Exemplo de payload UltraLight

```
s|on|bpm|82|spo2|97|temp|28.5|umid|63.2
```

### Exemplo de payload JSON (tópico sensor)

```json
{
  "bpm": 82,
  "spo2": 97,
  "temperatura": 28.5,
  "umidade": 63.2,
  "estado": 0,
  "alerta": "OK"
}
```

---

## Configuração FIWARE

### Variáveis de ambiente

```
BROKER_IP   = 34.135.50.230
MQTT_PORT   = 1883
IOT_PORT    = 4041
ORION_PORT  = 1026
API_KEY     = TEF
DEVICE_ID   = lamp003
ENTITY_ID   = urn:ngsi-ld:Lamp:003
SERVICE     = smart
SERVICEPATH = /
```

### Atributos provisionados no dispositivo

| object_id | name | type |
|---|---|---|
| `s` | state | Text |
| `bpm` | bpm | Integer |
| `spo2` | spo2 | Integer |
| `temp` | temperature | Float |
| `umid` | humidity | Float |

### Comandos registrados

| Comando | Ação |
|---|---|
| `on` | Liga o LED (GPIO 2) |
| `off` | Desliga o LED (GPIO 2) |

---

## Como Executar

### 1. Pré-requisitos

- [Arduino IDE](https://www.arduino.cc/en/software) ou [PlatformIO](https://platformio.org/)
- Bibliotecas necessárias:
  - `WiFi.h`
  - `PubSubClient`
  - `DHT sensor library`
  - `Adafruit GFX Library`
  - `Adafruit SSD1306`
- Stack FIWARE rodando (IoT Agent + Orion + MongoDB + Mosquitto)
- Opcional: [Wokwi](https://wokwi.com/) para simulação

### 2. Configurar o firmware

Edite as constantes no topo do arquivo `.ino`:

```cpp
const char* SSID        = "SUA_REDE";
const char* PASSWORD    = "SUA_SENHA";
const char* BROKER_MQTT = "34.135.50.230";
const int   BROKER_PORT = 1883;
```

### 3. Fazer upload

Conecte o ESP32, selecione a porta correta na IDE e faça o upload. O display OLED exibirá `CARE PLUS` na inicialização e, em seguida, as leituras em tempo real.

---

## Sequência de Deploy

Execute as requisições nesta ordem via Postman (collection `CarePlus_FIWARE_lamp003.postman_collection.json`):

```
1.  IoT Agent → Health Check IoT Agent         (GET  :4041/iot/about)
2.  Orion CB  → Versão do Orion                (GET  :1026/version)
3.  IoT Agent → Provisionar Service Group      (POST :4041/iot/services)
4.  IoT Agent → Provisionar Dispositivo lamp003(POST :4041/iot/devices)
5.  IoT Agent → Registrar Comandos lamp003     (POST :1026/v2/registrations)
6.  IoT Agent → Listar Todos os Dispositivos   (GET  :4041/iot/devices)
7.  Orion CB  → Listar Entidades               (GET  :1026/v2/entities)
──────────────────────────────────────────────
     ✅ Ligue o ESP32 e verifique a telemetria
──────────────────────────────────────────────
8.  IoT Agent → Ligar LED (on)                 (PATCH:1026/v2/entities/.../attrs)
9.  IoT Agent → Consultar Estado lamp003        (GET  :1026/v2/entities/urn:ngsi-ld:Lamp:003)
```

---

## Endpoints da API

Base URL: `http://34.135.50.230`

Headers obrigatórios em todas as requisições:
```
fiware-service:     smart
fiware-servicepath: /
```

| Método | Endpoint | Descrição |
|---|---|---|
| GET | `:4041/iot/about` | Health check IoT Agent |
| POST | `:4041/iot/services` | Provisionar service group |
| GET | `:4041/iot/services` | Listar service groups |
| DELETE | `:4041/iot/services/?resource=&apikey=TEF` | Remover service group |
| POST | `:4041/iot/devices` | Provisionar dispositivo |
| GET | `:4041/iot/devices` | Listar dispositivos |
| GET | `:1026/version` | Versão do Orion |
| GET | `:1026/v2/entities` | Listar entidades |
| GET | `:1026/v2/entities/urn:ngsi-ld:Lamp:003` | Consultar lamp003 |
| PATCH | `:1026/v2/entities/urn:ngsi-ld:Lamp:003/attrs` | Enviar comando on/off |
| POST | `:1026/v2/registrations` | Registrar comandos |

---

## Estrutura do Projeto

```
care-plus/
├── firmware/
│   └── care_plus.ino               # Código principal ESP32
├── docs/
│   ├── CarePlus_Arquitetura.drawio # Diagrama de arquitetura (draw.io)
│   └── README.md                   # Este arquivo
└── postman/
    └── CarePlus_FIWARE_lamp003.postman_collection.json
```

---

## Licença

Projeto acadêmico — FIAP · Internet das Coisas.
