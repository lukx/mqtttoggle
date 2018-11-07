/*
  mqtttoggle - ESP-01 based sensor for up to two wired switches
  Based on the great work by puuu at https://github.com/puuu/MQTT433gateway 

  The MIT License (MIT)
  Copyright (c) 2018 lukx

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "net-password.h"

#include <ESP8266WiFi.h>

#include <PubSubClient.h>

#ifndef myMQTT_USERNAME
#define myMQTT_USERNAME nullptr
#define myMQTT_PASSWORD nullptr
#endif


const char *ssid = mySSID;
const char *password = myWIFIPASSWD;
const char *mqttBroker = myMQTT_BROCKER;
const char *mqttUser = myMQTT_USERNAME;
const char *mqttPassword = myMQTT_PASSWORD;

#define BUTTON_1 0
#define BUTTON_2 1

#define PAYLOAD_HIGH "off"
#define PAYLOAD_LOW "on"
const int GPIO[] = {3, 2};

// we will set a reference value (LOW) to the RX pin,
// because if we would boot with one of the GPIOs connected
// to GND (=low), the ESP would start in flash or another mode
// this way, I can pull the reference current on RX pin later.
const int REFERENCE_CURRENT_PIN = 0;

int lastKnownStates[] = {-1, -1};

WiFiClient wifi;
PubSubClient mqtt(wifi);

const String chipName = String("ESP01_") + String(ESP.getChipId(), HEX);

const String globalTopic = "mqttswitch";
const String chipTopic = globalTopic + String("/") + chipName;

bool logMode = false;

void setupWifi() {
  delay(10);
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);  // Explicitly set station mode
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.println(F("WiFi connected"));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
}

void mqttCallback(const char *topic_, const byte *payload_,
                  unsigned int length) {
  Serial.print(F("Message arrived ["));
  Serial.print(topic_);
  Serial.print(F("] "));
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload_[i]);
  }
  Serial.println();
  // currently nothing is to done after receiving a mqtt msg
}

void gpioChangeCallback(const int button, const bool isHigh) {
  const int gpio = GPIO[button];
  
  Serial.print(F("GPIO "));
  Serial.print(gpio);
  Serial.print(F(" is now "));
  Serial.print(isHigh ? F("high"):F("low"));
  Serial.println();

  const String buttonTopic = chipTopic + String("/") + String(button);
  const String payload = (isHigh ? PAYLOAD_HIGH : PAYLOAD_LOW);
  mqtt.publish(buttonTopic.c_str(), payload.c_str(), true);
}

void reconnect() {
   // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (mqtt.connect(chipName.c_str(), mqttUser, mqttPassword,
                     chipTopic.c_str(), 0, true, "offline")) {
      Serial.println(F("connected"));
      mqtt.publish(chipTopic.c_str(), "online", true);
      // e.g.       mqtt.subscribe(chipTopic.c_str());
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(mqtt.state());
      Serial.println(F(" try again in 5 seconds"));
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  setupWifi();
  mqtt.setServer(mqttBroker, 1883);
  mqtt.setCallback(mqttCallback);
  
  // make the RX pin a normal GPIO
  pinMode(3, FUNCTION_3);

  pinMode(GPIO[BUTTON_1], INPUT);
  pinMode(GPIO[BUTTON_2], INPUT);

  // see the comment at the declaration of REFERENCE_CURRENT_PIN
  pinMode(REFERENCE_CURRENT_PIN, OUTPUT);
  digitalWrite(REFERENCE_CURRENT_PIN, LOW);
}

void processButton(int button) {
  int newVal = digitalRead(GPIO[button]);
  
  if (newVal == lastKnownStates[button]) {
    return;
  }
  
  lastKnownStates[button] = newVal;
  
  gpioChangeCallback(button,(newVal == HIGH));
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
  processButton(BUTTON_1);
  processButton(BUTTON_2);
}
