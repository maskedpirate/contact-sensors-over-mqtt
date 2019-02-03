/*
 * contact-sensors-over-mqtt
 * Monitors configured GPIO sensors for high/low and sends a message
 * to an MQTT topic.
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Mapping NodeMCU Ports to Arduino GPIO Pins
// Allows use of NodeMCU Port nomenclature in config.h
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12 
#define D7 13
#define D8 15

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const boolean static_ip = STATIC_IP;
IPAddress ip(IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);

const char* mqtt_broker = MQTT_BROKER;
const char* mqtt_clientId = MQTT_CLIENTID;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const String baseTopic = BASE_TOPIC;

String sAvailabilityTopic = baseTopic + "/availability";
char* availabilityTopic;

//const char* availabilityTopic = "shop/dc/availability";
const char* birthMessage = "online";
const char* lwtMessage = "offline";

int debounceTime = 1000;

typedef struct {
  String alias;
  char* topic;
  int   pin;
  char* logic;
  int   lastValue;
  unsigned long lastChangeTime;
} Sensor;

const int sensorsCount = 2;
Sensor sensors[sensorsCount] = {
  {
    .alias = "Miter Saw",
    .topic = "miter_saw",
    .pin = D1,
    .logic = "NO",
    .lastValue = 2,
    .lastChangeTime = 0,
  },
  {
    .alias = "Table Saw",
    .topic = "table_saw",
    .pin = D2,
    .logic = "NO",
    .lastValue = 2,
    .lastChangeTime = 0,
  }
};



WiFiClient espClient;
PubSubClient client(espClient);


// Wifi setup function
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (static_ip) {
    WiFi.config(ip, gateway, subnet);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print(" WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}



// Functions that check door status and publish an update when called
void publish_sensor_status(Sensor& sensor) {
  String topic = baseTopic + "/" + sensor.topic;
  char ctopic[topic.length() + 1];
  topic.toCharArray(ctopic, topic.length() +1);
  
  if (digitalRead(sensor.pin) == LOW) {
    if (sensor.logic == "NO") {
      Serial.print(sensor.alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(ctopic);
      Serial.println("...");
      client.publish(ctopic, "closed", true);
    }
    else if (sensor.logic == "NC") {
      Serial.print(sensor.alias);
      Serial.print(" open! Publishing to ");
      Serial.print(ctopic);
      Serial.println("...");
      client.publish(ctopic, "open", true);      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for sensor.alias[" + sensor.alias + "]! Not publishing...");
    }
  }
  else {
    if (sensor.logic == "NO") {
      Serial.print(sensor.alias);
      Serial.print(" open! Publishing to ");
      Serial.print(ctopic);
      Serial.println("...");
      client.publish(ctopic, "open", true);
    }
    else if (sensor.logic == "NC") {
      Serial.print(sensor.alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(ctopic);
      Serial.println("...");
      client.publish(ctopic, "closed", true);      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for sensor.alias[" + sensor.alias + "]! Not publishing...");
    }
  }
}


void check_sensor_status(Sensor& sensor) {
  int currentStatusValue = digitalRead(sensor.pin);
  if (currentStatusValue != sensor.lastValue) {
    unsigned int currentTime = millis();
    if (currentTime - sensor.lastChangeTime >= debounceTime) {
      publish_sensor_status(sensor);
      sensor.lastValue = currentStatusValue;
      sensor.lastChangeTime = currentTime;
    }
  }
}


// Function that publishes birthMessage
void publish_birth_message() {
  // Publish the birthMessage
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);
  Serial.print("\" to ");
  Serial.print(availabilityTopic);
  Serial.println("...");
  client.publish(availabilityTopic, birthMessage, true);
}

// Function that runs in loop() to connect/reconnect to the MQTT broker, and publish the current door statuses on connect
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_username, mqtt_password, availabilityTopic, 0, true, lwtMessage)) {
      Serial.println("Connected!");

      // Publish the birth message on connect/reconnect
      publish_birth_message();

      // Publish the current door status on connect/reconnect to ensure status is synced with whatever happened while disconnected
      for(int i=0; i<sensorsCount; i++) {
        publish_sensor_status(sensors[i]);
      }
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  availabilityTopic = new char[sAvailabilityTopic.length() + 1];
  sAvailabilityTopic.toCharArray(availabilityTopic, sAvailabilityTopic.length() +1);


  for(int i=0; i<sensorsCount; i++) {
    pinMode(sensors[i].pin, INPUT_PULLUP);
    sensors[i].lastValue = digitalRead(sensors[i].pin);
  }

  // Setup serial output, connect to wifi, connect to MQTT broker, set MQTT message callback
  Serial.begin(115200);

  Serial.println("Starting Dust Collector Sensors...");

  setup_wifi();
  client.setServer(mqtt_broker, 1883);
}

void loop() {
  // Connect/reconnect to the MQTT broker and listen for messages
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  for(int i=0; i<sensorsCount; i++) {
    check_sensor_status(sensors[i]);
  }
}
