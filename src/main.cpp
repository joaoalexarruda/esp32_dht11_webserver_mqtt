/*
 * File: main.cpp
 * Author: João Alex Arruda da Silva
 * Date: 2024-Jan
 * Description: This file contains the code for the ESP32 microcontroller
 *              that reads the temperature and humidity from the DHT11 sensor
 *              then calculates the moving average of the last readings
 *              and publishes the values to a MQTT broker. The code also
 *              displays the values on a local web server using the
 *              ESPAsyncWebServer library.
 */

// Include the necessary headers/libraries
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <deque>  // To be able to add and remove elements from both ends
#include <numeric>
#include "ESPAsyncWebServer.h"

// Wifi details: SSID and password
const char *ssid = "joaoalex1";
const char *password = "joao1579";

// MQTT broker details: IP address and port
const char *mqttServer = "192.168.29.165";
const int mqttPort = 1883;

// DHT11 sensor details: pin and type
const int DHTPIN = 4;
const int DHTTYPE = DHT11;
DHT dht(DHTPIN, DHTTYPE);

// Deque to store the last temperature and humidity readings
// and calculate the moving average
std::deque<float> temperatureReadings;
std::deque<float> humidityReadings;

// Size of the moving average, i.e., how many readings to consider
const int MOVING_AVERAGE_SIZE = 10;

// Time to keep track of the last reading
unsigned long lastReadingTime = 0;

// Create an instance of the AsyncWebServer class
// to serve the web page
AsyncWebServer server(80);

// WifiClient and PubSubClient instances,
// to connect to the MQTT broker
WiFiClient espClient;
PubSubClient client(espClient);

// Function to setup the Wi-Fi connection
void setupWifi() {
    delay(10);
    Serial.println("Connecting to " + String(ssid) + "...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting...");
    }
    Serial.println("Connected to " + String(ssid) + " network!");
}

// Function to reconnect to the MQTT broker
void reconnect() {
    while (!client.connected()) {
        Serial.print("Trying to connect to MQTT broker...");
        if (client.connect("ESP32Client")) {
            Serial.println("Connected!");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// Read the temperature from the DHT11 sensor and
// calculate the moving average of the last readings
float readTemperatureAndCalculateMovingAverage() {
    float currentTemperature = dht.readTemperature();
    // If the reading is NaN, return the last valid reading
    if (isnan(currentTemperature)) {
        return temperatureReadings.back();
    }

    // Add the current reading to the readings deque
    temperatureReadings.push_back(currentTemperature);

    // If we have more readings than we want to consider for the moving average,
    // remove the oldest one
    if (temperatureReadings.size() > MOVING_AVERAGE_SIZE) {
        temperatureReadings.pop_front();
    }

    // Calculate the sum of the readings
    float sum = std::accumulate(temperatureReadings.begin(),
                                temperatureReadings.end(), 0.0f);

    // Calculate and return the moving average
    return sum / temperatureReadings.size();
}

// Read the humidity from the DHT11 sensor and
// calculate the moving average of the last readings
float readHumidityAndCalculateMovingAverage() {
    float currentHumidity = dht.readHumidity();
    // If the reading is NaN, return the last valid reading
    if (isnan(currentHumidity)) {
        return humidityReadings.back();
    }

    // Add the current reading to the readings deque
    humidityReadings.push_back(currentHumidity);

    // If we have more readings than we want to consider for the moving average,
    // remove the oldest one
    if (humidityReadings.size() > MOVING_AVERAGE_SIZE) {
        humidityReadings.pop_front();
    }

    // Calculate the sum of the readings
    float sum =
        std::accumulate(humidityReadings.begin(), humidityReadings.end(), 0.0f);

    // Calculate and return the moving average
    return sum / humidityReadings.size();
}

// HTML code for the web page served by the ESP32 microcontroller
// The placeholders %TEMPERATURE% and %HUMIDITY% will be replaced by the actual
// values
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<script src="https://kit.fontawesome.com/1b6e98d141.js" crossorigin="anonymous"></script>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
  body {
    font-family: Arial, sans-serif;
    background-color: #121212;
    color: #ffffff;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: 100vh;
    }
    .header { display: flex; align-items: center; gap: 20px; }
    .header img { width: 160px; height: auto; }
    .header h2 { font-size: 2.5rem; color: #ffffff; }
    p { background-color: #1f1f1f; border-radius: 5px; padding: 20px; margin: 10px; width: 80vw; display: flex; align-items: center; justify-content: space-between; }
    .units { font-size: 1rem; }
    .fa-solid, .fas { margin-right: 10px; }
    .dht-labels{
      flex-grow: 1;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    footer {
      position: fixed;
      left: 0;
      bottom: 0;
      width: 100vw;
      background-color: #1f1f1f;
      color: white;
      text-align: center;
      padding: 10px 0;
    }
    footer a {
      color: white;
      text-decoration: none;
    }
    footer a:hover {
      color: #ddd;
    }
  </style>
</head>
<body>
  <div class="header"><img src="https://i.imgur.com/23DiEOf.png"><h2>ESP32</h2></div>
  <p>
    <i class="fa fa-temperature-high" style="color:#9e0505;"></i> 
    <span class="dht-labels">  TEMPERATURE</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <span class="units">&deg;C</span>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">  HUMIDITY</span>
    <span id="humidity">%HUMIDITY%</span>
    <span class="units">&percnt;</span>
  </p>
  <footer>
    <a href="https://github.com/joaoalexarruda" target="_blank"><i class="fab fa-github fa-2x"></i></a>
  </footer>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

// Function to replace the placeholders in the HTML code
// with the actual values
String processor(const String &var) {
    // Serial.println(var);
    if (var == "TEMPERATURE") {
        return String(readTemperatureAndCalculateMovingAverage());
    } else if (var == "HUMIDITY") {
        return String(readHumidityAndCalculateMovingAverage());
    }
    return String();
}

// Setup function
// This function is called only once when the microcontroller starts
void setup() {
    Serial.begin(115200);                    // Initialize serial communication
    dht.begin();                             // Initialize DHT sensor
    setupWifi();                             // Setup Wi-Fi connection
    client.setServer(mqttServer, mqttPort);  // Setup MQTT broker

    // Setup the web server, and define the routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html, processor);
    });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(
            200, "text/plain",
            String(readTemperatureAndCalculateMovingAverage()).c_str());
    });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(
            200, "text/plain",
            String(readHumidityAndCalculateMovingAverage()).c_str());
    });

    // Start server
    server.begin();
}

// Loop function
// This function is called repeatedly
void loop() {
    // Check if the client is connected to the MQTT broker
    if (!client.connected()) {
        reconnect();
    }

    // Every 3 seconds, read the temperature and humidity from the DHT11 sensor,
    // calculate the moving average, and publish the values to the MQTT broker
    // This interval allows the microcontroller to perform other tasks
    // and not be constantly occupied with reading sensor data.
    unsigned long currentMillis = millis();
    if (currentMillis - lastReadingTime >= 3000) {
        // Read the temperature and humidity from the DHT11 sensor and
        // calculate the moving average
        float avgTemperature = readTemperatureAndCalculateMovingAverage();
        float avgHumidity = readHumidityAndCalculateMovingAverage();

        // Convert the float values to strings
        char avgTempStr[8];
        dtostrf(avgTemperature, 2, 2, avgTempStr);
        char avgHumStr[8];
        dtostrf(avgHumidity, 2, 2, avgHumStr);

        // Name of the topics to publish the values
        char avgTempTopic[] = "esp32/moving_average_temperature";
        char avgHumTopic[] = "esp32/moving_average_humidity";

        // Publish the values to the MQTT broker
        client.publish(avgTempTopic, avgTempStr);
        client.publish(avgHumTopic, avgHumStr);

        // Print the values and topics to the serial monitor for debugging
        Serial.println();
        Serial.println("+~~~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~+");
        Serial.println("|               DEBUGGING               |");
        Serial.println("+~~~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~>~~~+");
        Serial.printf("| Local IP Address  | %-17s |\n",
                      WiFi.localIP().toString().c_str());
        Serial.println("+~~~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~+");
        Serial.println("| Parameter         | Value             |");
        Serial.println("+-------------------+-------------------+");
        Serial.printf("| Moving Avg Temp.  | %-17.2f |\n", avgTemperature);
        Serial.printf("| Moving Avg Humid. | %-17.2f |\n", avgHumidity);
        Serial.println("+-------------------+-------------------+");
        Serial.println("|            Published Topic            |");
        Serial.println("+-------------------+-------------------+");
        Serial.println("| " + String(avgTempTopic) + "      |");
        Serial.println("| " + String(avgHumTopic) + "         |");
        Serial.println("+-------------------+-------------------+");

        // Update the last reading time
        lastReadingTime = currentMillis;
    }
}