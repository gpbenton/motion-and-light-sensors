#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <secrets.h>

WiFiClient wifiClient;

// mqttClient parameters
PubSubClient mqttClient(wifiClient);

//MQTT last attemps reconnection number
unsigned long lastReconnectAttempt = 0;
unsigned long reconnectStart = 0;
unsigned long lastDarkMessage = 0;
bool noticeSent = false;
bool firsttime = true;

// Motion Sensor Pin
const int MOTIONSENSORPIN = D1;  // D5 on NodeMCU is GPIO14 on 202
boolean motionDetected = false;
const int LIGHT_LEVEL_PIN = D8;
boolean lightlevel = false;

#define TRACE true

void trc(String msg){
  if (TRACE) {
    Serial.println(msg);
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  trc("Connecting to ");
  trc(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    trc(".");
  }
  trc("WiFi connected");
}

boolean reconnect() {

  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    firsttime = true;
      trc("Using default mqtt address");
      mqttClient.setServer(mqtt_server, 1883);

      // Attempt to connect
      if (mqttClient.connect("433toMQTTto433", "sensor/433/status", 1, true,
                             "offline")) {
        // Once connected, publish an announcement...
        mqttClient.publish("sensor/433/status","online", true);
        // and turn off the LED
        digitalWrite(LED_BUILTIN, HIGH);
        trc("connected");
        //Subscribing to topic(s)
        //subscribing(subjectMQTTtoX);
      } else {
        trc("failed, rc=");
        trc(String(mqttClient.state()));
      }

  }
  return mqttClient.connected();
}

//send MQTT data dataStr to topic topicNameSend
void sendMQTT(String topicNameSend, String dataStr, boolean retain){

  char topicStrSend[100];
  topicNameSend.toCharArray(topicStrSend,100);
  char dataStrSend[200];
  dataStr.toCharArray(dataStrSend,200);
  mqttClient.publish(topicStrSend,dataStrSend, retain);
  trc("sending ");
  trc(dataStr);
  trc("to ");
  trc(topicNameSend);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  //Launch serial for debugging purposes
  Serial.begin(115200);
  //Begining wifi connection
  setup_wifi();
  delay(1500);
  lastReconnectAttempt = 0;

  pinMode(MOTIONSENSORPIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);   // Turn on LED until connected


}

void loop() {
  unsigned long now = millis();

  //MQTT mqttClient connexion management
  if (!mqttClient.connected()) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (!noticeSent && reconnectStart == 0) {
        reconnectStart = millis();
      }
      trc("mqttClient mqtt not connected, trying to connect");
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
        reconnectStart = 0;
        noticeSent = false;
      } else if (!noticeSent && ((now - reconnectStart) > 60000)) {
        trc("Sending alert");
        noticeSent = true;

      }
    }
  } else {
    // MQTT loop
    mqttClient.loop();
  }
  
  if (mqttClient.connected()) {
    //Light level
    // Only send once a minute to avoid spamming when light 
    // level is on the limit
    if (now < lastDarkMessage // Check for wrap around
        ||
        now > (lastDarkMessage + 60000)
        || firsttime == true) {
      boolean currentLight = (digitalRead(LIGHT_LEVEL_PIN) == HIGH);
      if (lightlevel && !currentLight) {
        sendMQTT("sensor/433/dark", "OFF", false);
        lightlevel = currentLight;
        lastDarkMessage = now;
      } else if (!lightlevel && currentLight) {
        sendMQTT("sensor/433/dark", "ON", false);
        lightlevel = currentLight;
        lastDarkMessage = now;
      } else if (firsttime == true) {
        sendMQTT("sensor/433/dark", currentLight?"ON":"OFF", false);
        lightlevel = currentLight;
        lastDarkMessage = now;
      }
    }

  // Moton Detection
    boolean currentMotion = (digitalRead(MOTIONSENSORPIN) == HIGH);
    if (motionDetected && !currentMotion) {
      sendMQTT("sensor/433/motion", "OFF", false);
      motionDetected = currentMotion;
    } else if (!motionDetected && currentMotion) {
      sendMQTT("sensor/433/motion", "ON", false);
      motionDetected = currentMotion;
    } else if (firsttime == true) {
      sendMQTT("sensor/433/motion", currentMotion?"ON":"OFF", false);
      motionDetected = currentMotion;
    }
    firsttime = false;
  }
}