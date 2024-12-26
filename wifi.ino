#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>  // AHT20/AHT21 Sensor
#include "ScioSense_ENS160.h" // ENS160 library
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_AHTX0 aht;
ScioSense_ENS160 ens160(0x53); // ENS160 I2C address 0x53 (ENS160+AHT21)

// Variablen für Temperatur und Luftfeuchtigkeit
float tempC;
float tempF;
float humidity;

// WiFi und MQTT Server Zugangsdaten
const char* ssid = "OhneGehtNichtOben";
const char* password = "musiker12";

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
    delay(10);
    Serial.println("Verbindung zu WiFi herstellen...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi verbunden");
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Verbindung zum MQTT-Server herstellen...");
        if (client.connect("ESP32Client")) {
            Serial.println("verbunden");
        } else {
            Serial.print("Fehler, rc=");
            Serial.print(client.state());
            Serial.println(" - Neuer Versuch in 5 Sekunden");
            delay(5000);
        }
    }
}

// Funktion zum direkten Lesen eines 16-Bit-Registers
uint16_t readRegister16(uint8_t reg) {
    Wire.beginTransmission(0x53);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(0x53, (uint8_t)2);
    uint16_t value = (Wire.read() << 8) | Wire.read();
    return value;
}

// Ermittelt die Anzahl der Balken anhand des RSSI-Wertes
uint8_t getWifiBars(long rssi) {
    // Anhand typischer RSSI-Werte (anpassbar)
    if (rssi > -60) return 4;  // sehr gutes Signal
    else if (rssi > -67) return 3; // gutes Signal
    else if (rssi > -74) return 2; // mittleres Signal
    else if (rssi > -81) return 1; // schwaches Signal
    else return 0;                 // sehr schwach/kein Signal
}

// Zeichnet die WLAN-Balken oben rechts
void drawWifiBars(uint8_t bars) {
    // Position oben rechts, z.B. ab x=100, y=0
    int xBase = 106;
    int yBase = 0;
    int barWidth = 3;
    int barSpacing = 2; 
    // Balkenhöhen von unten nach oben
    // z.B. jeder Balken 2 Pixel höher als der nächste
    // Höhe für Balken:
    // 1. Balken (unterste): Höhe 3 Pixel
    // 2. Balken: Höhe 5 Pixel
    // 3. Balken: Höhe 7 Pixel
    // 4. Balken: Höhe 9 Pixel
    // Die Balken werden von unten nach oben gezeichnet.
    // Offset von unten (ausgehend von y=0 oben):
    // Wir können von unten (y=64) hoch rechnen, aber da oben rechts gezeichnet wird,
    // nehmen wir einfach yBase als Start und zeichnen nach unten.
    // Alternativ drehen wir die Logik um und zeichnen von oben nach unten.
    // Hier zeichnen wir einfach stapelnd von unten nach oben:
    // Da das Display 64 Pixel hoch ist, platzieren wir die Balken oben rechts:
    // Wir sagen einfach, der unterste Balken beginnt bei y=12. 
    // Dann jeder Balken weiter oben.
    // Anpassung möglich je nach Geschmack.
    int bottomY = 8; 
    // bars = Anzahl der Balken, von unten nach oben

    // Balken 1 (falls bars >= 1)
    if (bars >= 1) {
      display.fillRect(xBase, bottomY, barWidth, 3, SSD1306_WHITE);
    }
    // Balken 2 (falls bars >= 2)
    if (bars >= 2) {
      display.fillRect(xBase + barWidth + barSpacing, bottomY - 2, barWidth, 5, SSD1306_WHITE);
    }
    // Balken 3 (falls bars >= 3)
    if (bars >= 3) {
      display.fillRect(xBase + 2*(barWidth + barSpacing), bottomY - 4, barWidth, 7, SSD1306_WHITE);
    }
    // Balken 4 (falls bars >= 4)
    if (bars == 4) {
      display.fillRect(xBase + 3*(barWidth + barSpacing), bottomY - 6, barWidth, 9, SSD1306_WHITE);
    }
}

void setup() {
    Serial.begin(9600);
    setup_wifi();
    client.setServer("147.185.221.23", 36555);

    Serial.println("------------------------------------------------------------");
    Serial.println("ENS160 und AHT20/AHT21 - Sensoren initialisieren");
    Serial.println("------------------------------------------------------------");
    Wire.begin(16, 17); // SDA an D16, SCL an D17

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 konnte nicht gefunden werden.");
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Init...");
    display.display();

    if (ens160.begin()) {
        Serial.println("ENS160 erfolgreich initialisiert.");
        ens160.setMode(ENS160_OPMODE_STD);
    } else {
        Serial.println("ENS160 konnte nicht initialisiert werden.");
    }

    if (!aht.begin()) {
        Serial.println("AHT konnte nicht gefunden werden! Überprüfen Sie die Verkabelung.");
        while (1) delay(10);
    } else {
        Serial.println("AHT10 oder AHT20 gefunden.");
    }

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Verbinde WLAN...");
    display.display();
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Verbinde MQTT...");
    display.display();
    while (!client.connected()) {
        if (client.connect("ESP32Client")) {
            // verbunden
        } else {
            delay(500);
        }
    }
    display.clearDisplay();
}
float tempCorrection = -2.6; // Differenz zur Referenztemperatur
float humidityCorrection = 5.0; // Differenz zur Referenzfeuchtigkeit

unsigned long lastMQTTReconnectAttempt = 0; // Timer für MQTT-Reconnect
const long mqttReconnectInterval = 5000;   // 5 Sekunden

void loop() {
    // MQTT-Verbindung alle 5 Sekunden überprüfen
    if (!client.connected() && (millis() - lastMQTTReconnectAttempt > mqttReconnectInterval)) {
        lastMQTTReconnectAttempt = millis();
        reconnect(); // Versuch, MQTT erneut zu verbinden
    }

    client.loop(); // MQTT-Client aktualisieren

    // AHT20/AHT21 Daten auslesen
    sensors_event_t humidity1, temp;
    aht.getEvent(&humidity1, &temp);
    tempC = temp.temperature + tempCorrection;
    tempF = temp.temperature * 1.8 + 32;
    humidity = humidity1.relative_humidity + humidityCorrection;

    ens160.set_envdata(tempC, humidity);

    int aqi = -1;
    int tvoc = -1;
    int eco2 = -1;

    if (ens160.available()) {
        ens160.measure(true);  
        ens160.measureRaw(true);  
        aqi = ens160.getAQI();
        tvoc = ens160.getTVOC();
        eco2 = ens160.geteCO2();
    }

    // Werte auf Display ausgeben
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(2,4);
    display.print("Temperatur:");
    display.print(tempC, 2);
    display.print("C");
    display.setCursor(2,16);
    display.print("Feuchtigkeit:");
    display.print(humidity, 2);
    display.print("%");

    long rssi = WiFi.RSSI();
    uint8_t bars = getWifiBars(rssi);
    drawWifiBars(bars);

    display.setCursor(2,28);
    display.print("AQI: ");
    display.print(aqi);

    display.setCursor(2,40);
    display.print("TVOC: ");
    display.print(tvoc);
    display.print(" ppb");

    display.setCursor(2,52);
    display.print("eCO2: ");
    display.print(eco2);
    display.print(" ppm");

    display.display();

    // Optional: Sensorwerte weiterhin an MQTT senden, falls verbunden
    if (client.connected()) {
        client.publish("sensor/temperatureC", (String(tempC) + " °C").c_str());
        client.publish("sensor/humidity", (String(humidity) + " % rH").c_str());
        client.publish("sensor/air_quality/aqi", (String(aqi) + " AQI").c_str());
        client.publish("sensor/air_quality/tvoc", (String(tvoc) + " ppb").c_str());
        client.publish("sensor/air_quality/eco2", (String(eco2) + " ppm").c_str());
    }

    delay(1000);
}