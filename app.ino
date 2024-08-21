// Bibliotecas
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


// Estruturas
struct Sensor {
    String name;
    float value;
    String unit;
    String date;
};

struct SensorsData
{
    Sensor temperature;
    Sensor orp;
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

// Inicialização de componentes
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Variávies para controle de tempo
unsigned long lastSendTime = 0;
const unsigned long interval = 5000;

// Assinaturas das funções auxiliares
SensorsData getSensorsData();
void sendSensorsData();
void toggleRelays();
void setLimits();

void setup()
{
    Serial.begin(115200);
    espClient.setInsecure();

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
    if (currentMillis - lastSendTime >= interval) {
        lastSendTime = currentMillis;
        sendSensorsData();
    }
}

// Funções auxiliares
void callback(char *topic, byte *payload, unsigned int length)
{
    if (strcmp(topic, topic_sensors_data) == 0)
    {

    }
    else if (strcmp(topic, topic_limits) == 0)
    {
        setLimits();
    }
    else if (strcmp(topic, topic_relays))
    {
        toggleRelays();
    }
    else
    {
        Serial.println("Tópico desconhecido");
    }
}

SensorsData getSensorsData() {
    SensorsData data;
    
    data.temperature.name = "Temperatura";
    data.temperature.value = 25.3;
    data.temperature.date = "";

    data.orp.name = "ORP";
    data.orp.value = 15.8;
    data.orp.date = "";


    return data;
}
void sendSensorsData() {
    SensorsData data = getSensorsData();

    StaticJsonDocument<200> json;

    JsonObject temperature = json.createNestedObject("temperature");
    temperature["name"] = data.temperature.name;
    temperature["value"] = data.temperature.value;
    temperature["date"] = data.temperature.date;

    JsonObject orp = json.createNestedObject("orp");
    orp["name"] = data.orp.name;
    orp["value"] = data.orp.value;
    orp["date"] = data.orp.date;

    char buffer[512];
    serializeJson(json, buffer);

    client.publish(topic_sensors_data, buffer);
}
void toggleRelays() {}
void setLimits() {}