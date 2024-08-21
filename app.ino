// Bibliotecas
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <RTClib.h>

#include <Ezo_i2c.h>
#include <Ezo_i2c_util.h>
#include <iot_cmd.h>

#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_I2CRegister.h>
#include <Adafruit_SPIDevice.h>
#include <Adafruit_MCP23X08.h>
#include <Adafruit_MCP23X17.h>

// Estruturas
struct SensorLimits
{
    float min;
    float max;
};

struct Sensor
{
    String name;
    float value;
    String unit;
    String date;
};

struct Sensors
{
    Sensor temperature;
    Sensor orp;
    Sensor ph;
    Sensor salinity;
    Sensor condutivity;
};

// Constantes
const char *ssid = "CLARO_2G6469F3";
const char *password = "C96469F3";

const char *mqtt_broker = "fad63335d02f44bd88cb9765cd98f853.s1.eu.hivemq.cloud";
const char *mqtt_username = "thom";
const char *mqtt_password = "2209";
const int mqtt_port = 8883;

const char *topic_sensors_data = "aqua/sensors";
const char *topic_limits = "aqua/limits";
const char *topic_relays = "aqua/relays";

unsigned long lastSendTime = 0;
const unsigned long interval = 5000;

// Limites padrões
SensorLimits tempLimits = {15.0, 30.0};         // Limites padrão para temperatura (0°C a 50°C)
SensorLimits orpLimits = {-1000.0, 1000.0};     // Limites padrão para ORP
SensorLimits phLimits = {0.0, 14.0};            // Limites padrão para pH (0 a 14)
SensorLimits salinityLimits = {0.0, 100.0};     // Limites padrão para salinidade
SensorLimits condutivityLimits = {0.0, 2000.0}; // Limites padrão para condutividade

// Inicialização de componentes
WiFiClientSecure espClient;
PubSubClient client(espClient);
RTC_DS3231 rtc;

Ezo_board DO = Ezo_board(97, "DO");    // Oxigênio Dissolvido
Ezo_board ORP = Ezo_board(98, "ORP");  // Oxidação-redução pontencial
Ezo_board PH = Ezo_board(99, "PH");    // pH
Ezo_board RTD = Ezo_board(102, "RTD"); // Temperatura
Ezo_board EC = Ezo_board(100, "EC");   // Condutividade e Salinidade

// Assinaturas das funções auxiliares
Sensors getSensorsData();
void sendSensorsData();
void toggleRelays();
void setLimits(const char *payload);
void callback(char *topic, byte *payload, unsigned int length);

void setup()
{
    Serial.begin(115200);
    espClient.setInsecure();

    // Inicialização do relógio
    if (!rtc.begin())
    {
        Serial.println("Não foi possível encontrar o módulo RTC");
        while (1)
            ;
    }

    if (rtc.lostPower())
    {
        Serial.println("RTC perdeu energia, configurando a data e hora...");
        rtc.adjust(DateTime(2024, 8, 21, 10, 0, 0));
    }

    // Inicialização do WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Conectando ao WiFi...");
    }
    Serial.printf("Conectado na rede: %s\n", ssid);

    // Inicialização do MQTT Broker
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);

    while (!client.connected())
    {
        String clientId = "esp-" + String(WiFi.macAddress());
        if (client.connect(clientId.c_str(), mqtt_username, mqtt_password))
        {
            Serial.printf("%s conectado ao broker", clientId.c_str());
        }
        else
        {
            Serial.print("Falha ao conectar no broker");
            Serial.print(client.state());
        }

        client.subscribe(topic_sensors_data);
        client.subscribe(topic_limits);
        client.subscribe(topic_relays);
    }
}

void loop()
{
    client.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= interval)
    {
        lastSendTime = currentMillis;
        sendSensorsData();
    }
}

// Funções auxiliares
void callback(char *topic, byte *payload, unsigned int length)
{
    String payloadStr;
    for (unsigned int i = 0; i < length; i++)
    {
        payloadStr += (char)payload[i];
    }
    if (strcmp(topic, topic_limits) == 0)
    {
        setLimits(payloadStr.c_str());
    }
    else if (strcmp(topic, topic_relays) == 0)
    {
        toggleRelays();
    }
    else
    {
        Serial.println("Tópico desconhecido");
    }
}

Sensors getSensorsData()
{
    Sensors data;

    DateTime now = rtc.now();
    String date = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + " " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

    // Temperatura
    RTD.send_read_cmd();
    if (RTD.receive_read_cmd() == Ezo_board::SUCCESS)
    {
        data.temperature.name = "Temperatura";
        data.temperature.unit = "C";
        data.temperature.date = date;
        data.temperature.value = RTD.get_last_received_reading();

        if (data.temperature.value < tempLimits.min || data.temperature.value > tempLimits.max)
        {
            Serial.println("Temperatura fora dos limites!");
        }

        Serial.print("Temperatura: ");
        Serial.println(data.temperature.value);
    }
    else
    {
        Serial.println("Erro ao ler o sensor de temperatura.");
    }

    // Oxidação-redução pontencial
    ORP.send_read_cmd();
    if (ORP.receive_read_cmd() == Ezo_board::SUCCESS)
    {
        data.orp.name = "ORP";
        data.orp.unit = "mV";
        data.orp.date = date;
        data.orp.value = ORP.get_last_received_reading();

        if (data.orp.value < orpLimits.min || data.orp.value > orpLimits.max)
        {
            Serial.println("ORP fora dos limites!");
        }

        Serial.print("ORP: ");
        Serial.println(data.orp.value);
    }
    else
    {
        Serial.println("Erro ao ler o sensor de ORP.");
    }

    // pH
    PH.send_read_cmd();
    if (PH.receive_read_cmd() == Ezo_board::SUCCESS)
    {
        data.ph.name = "pH";
        data.ph.unit = "";
        data.ph.date = date;
        data.ph.value = PH.get_last_received_reading();

        if (data.ph.value < phLimits.min || data.ph.value > phLimits.max)
        {
            Serial.println("pH fora dos limites!");
        }

        Serial.print("pH: ");
        Serial.println(data.ph.value);
    }
    else
    {
        Serial.println("Erro ao ler o sensor de pH.");
    }

    // Condutividade
    EC.send_read_cmd();
    if (EC.receive_read_cmd() == Ezo_board::SUCCESS)
    {
        data.condutivity.name = "Condutividade";
        data.condutivity.unit = "µS/cm";
        data.condutivity.date = date;
        data.condutivity.value = EC.get_last_received_reading();

        if (data.condutivity.value < condutivityLimits.min || data.condutivity.value > condutivityLimits.max)
        {
            Serial.println("Condutividade fora dos limites!");
        }

        Serial.print("Condutividade: ");
        Serial.println(data.condutivity.value);

        // Calcular Salinidade a partir da Condutividade
        data.salinity.name = "Salinidade";
        data.salinity.value = 0.5 * (data.condutivity.value / 1000); // Conversão aproximada
        data.salinity.unit = "ppt";
        data.salinity.date = date;

        if (data.salinity.value < salinityLimits.min || data.salinity.value > salinityLimits.max)
        {
            Serial.println("Salinidade fora dos limites!");
        }

        Serial.print("Salinidade: ");
        Serial.println(data.salinity.value);
    }
    else
    {
        Serial.println("Erro ao ler o sensor de condutividade.");
    }

    return data;
}
void sendSensorsData()
{
    Sensors data = getSensorsData();

    StaticJsonDocument<200> json;

    JsonObject temperature = json.createNestedObject("temperature");
    temperature["name"] = data.temperature.name;
    temperature["value"] = data.temperature.value;
    temperature["date"] = data.temperature.date;

    JsonObject orp = json.createNestedObject("orp");
    orp["name"] = data.orp.name;
    orp["value"] = data.orp.value;
    orp["date"] = data.orp.date;

    JsonObject ph = json.createNestedObject("ph");
    ph["name"] = data.ph.name;
    ph["value"] = data.ph.value;
    ph["date"] = data.ph.date;

    JsonObject salinity = json.createNestedObject("salinity");
    salinity["name"] = data.salinity.name;
    salinity["value"] = data.salinity.value;
    salinity["date"] = data.salinity.date;

    JsonObject condutivity = json.createNestedObject("condutivity");
    condutivity["name"] = data.condutivity.name;
    condutivity["value"] = data.condutivity.value;
    condutivity["date"] = data.condutivity.date;

    char buffer[512];
    serializeJson(json, buffer);

    client.publish(topic_sensors_data, buffer);
}
void setLimits(const char *payload)
{
    // Exemplo de mensagem JSON para limites
    // {"temperature": {"min": 0.0, "max": 50.0}, "orp": {"min": -1000.0, "max": 1000.0}, ...}

    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        Serial.println("Erro ao desserializar JSON dos limites");
        return;
    }

    if (doc.containsKey("temperature"))
    {
        JsonObject tempLimitsObj = doc["temperature"];
        tempLimits.min = tempLimitsObj["min"];
        tempLimits.max = tempLimitsObj["max"];
    }

    if (doc.containsKey("orp"))
    {
        JsonObject orpLimitsObj = doc["orp"];
        orpLimits.min = orpLimitsObj["min"];
        orpLimits.max = orpLimitsObj["max"];
    }

    if (doc.containsKey("ph"))
    {
        JsonObject phLimitsObj = doc["ph"];
        phLimits.min = phLimitsObj["min"];
        phLimits.max = phLimitsObj["max"];
    }

    if (doc.containsKey("salinity"))
    {
        JsonObject salinityLimitsObj = doc["salinity"];
        salinityLimits.min = salinityLimitsObj["min"];
        salinityLimits.max = salinityLimitsObj["max"];
    }

    if (doc.containsKey("condutivity"))
    {
        JsonObject condutivityLimitsObj = doc["condutivity"];
        condutivityLimits.min = condutivityLimitsObj["min"];
        condutivityLimits.max = condutivityLimitsObj["max"];
    }

    Serial.println("Limites atualizados");
}
void toggleRelays() {}