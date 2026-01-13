/**
 * ATOMS3_EnvIII_Monitor.ino
 * ATOMS3 + Unit EnvIII (SHT30 + QMP6988)
 * 
 * Features:
 * - EnvIII (I2C) for Temp, Humidity, Pressure
 * - Robust WiFi Connection
 * - Ambient IoT Data Logging (Avg of 1 min)
 *   - Field 1: Temp
 *   - Field 2: Humidity
 *   - Field 3: Pressure
 * - Display:
 *   - Top: Current Values (Small Font, Colored)
 *   - Bottom: Graph (90 min history)
 *   - Multi-Axis Graph (3 scales)
 */

#include <M5Unified.h>
#include <M5UnitENV.h>
#include <Ambient.h>
#include <WiFi.h>
#include <Wire.h>

// --- Configuration ---
#define AMBIENT_CHANNEL_ID XXXXX
#define AMBIENT_WRITE_KEY "XXXXXXXXXXXXXXXX"

// WiFi Credentials
#define WIFI_SSID "XXXXXX"
#define WIFI_PASS "XXXXXXXX"

// Graph Settings
#define MAX_HISTORY 90

// Colors
#define COLOR_TEMP   WHITE
#define COLOR_HUM    YELLOW
#define COLOR_PRESS  CYAN

// --- Globals ---
SHT3X sht3x;
QMP6988 qmp;
Ambient ambient;
WiFiClient client;

// Display Dimensions
int screenWidth = 128;
int screenHeight = 128;

// Graph Margins
int graphMinX = 35;  
int graphMaxX = 126; // Inclusive
int graphTopY = 40;  
int graphHeight = 0;

// Sensor Data
struct SensorData {
    float temp;
    float hum;
    float press;
    bool updated;
} currentData = {0, 0, 0, false};

// Accumulator for 1-minute average
struct SensorAccumulator {
    float tempSum;
    float humSum;
    float pressSum;
    int count;
} accData = {0, 0, 0, 0};

// History for Graph
float historyTemp[MAX_HISTORY];
float historyHum[MAX_HISTORY];
float historyPress[MAX_HISTORY];
int historyCount = 0; 

// Timing
unsigned long lastMeasureTime = 0;
unsigned long lastSendTime = 0;
const unsigned long MEASURE_INTERVAL = 1000; // 1 second
const unsigned long SEND_INTERVAL = 60000;   // 1 minute

// --- WiFi Functions ---

void WiFiConnect() {
    int lpcnt = 0;
    int lpcnt2 = 0;

    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.fillScreen(BLACK);
    M5.Display.drawString("Connecting WiFi...", screenWidth/2, screenHeight/2);
    
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lpcnt++;
        Serial.print(".");
        M5.Display.fillCircle(screenWidth/2, screenHeight/2 + 20, 5, (lpcnt % 2 == 0) ? WHITE : BLACK);

        if (lpcnt > 6) {
            WiFi.disconnect(true, true);
            delay(1000);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            Serial.println("\nResetted WiFi, retrying...");
            lpcnt = 0;
            lpcnt2++;
        }

        if (lpcnt2 > 3) {
            Serial.println("\nWiFi Connection failed. Restarting...");
            M5.Display.fillScreen(RED);
            M5.Display.drawString("WiFi Fail -> Restart", screenWidth/2, screenHeight/2);
            delay(2000);
            ESP.restart();
        }
    }

    Serial.println("\nWiFi Connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Re-init Ambient
    client.setTimeout(3000); 
    ambient.begin(AMBIENT_CHANNEL_ID, AMBIENT_WRITE_KEY, &client);
    
    M5.Display.fillScreen(BLACK);
}

// --- Setup Functions ---

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(500); 
    Serial.println("\nATOMS3 EnvIII Monitor Starting...");

    M5.Display.setRotation(0);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    Serial.printf("Screen: %dx%d\n", screenWidth, screenHeight);
    
    // Layout Calculation
    // Top 40px for Text
    graphTopY = 40;
    graphHeight = 125 - graphTopY;
    
    M5.Display.fillScreen(BLACK);

    // I2C Init (SDA=2, SCL=1 for ATOMS3)
    Wire.begin(2, 1);

    // EnvIII Init (SHT30 + QMP6988)
    if (!sht3x.begin(&Wire, SHT3X_I2C_ADDR, 2, 1, 400000U)) {
        Serial.println("Couldn't find SHT3X");
    }

    if (!qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 2, 1, 400000U)) {
        Serial.println("Couldn't find QMP6988");
    }

    Serial.println("EnvIII Initialized");

    // Robust WiFi Connect
    WiFiConnect();
    
    delay(1000);
    M5.Display.fillScreen(BLACK);
}

void updateAccumulator() {
    accData.tempSum += currentData.temp;
    accData.humSum += currentData.hum;
    accData.pressSum += currentData.press;
    accData.count++;
}

void updateGraphHistory(float temp, float hum, float press) {
    if (historyCount < MAX_HISTORY) {
        historyTemp[historyCount] = temp;
        historyHum[historyCount] = hum;
        historyPress[historyCount] = press;
        historyCount++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            historyTemp[i] = historyTemp[i+1];
            historyHum[i] = historyHum[i+1];
            historyPress[i] = historyPress[i+1];
        }
        historyTemp[MAX_HISTORY-1] = temp;
        historyHum[MAX_HISTORY-1] = hum;
        historyPress[MAX_HISTORY-1] = press;
    }
}

// Helper to draw points for one series
void drawSeries(float* data, uint16_t color, float plotMin, float plotMax, int pointRadius, int labelOffsetY, bool drawLabels) {
    if (historyCount == 0) return;

    float range = plotMax - plotMin;
    if (range <= 0) range = 1.0;

    // Draw Points
    for (int i = 0; i < historyCount; i++) {
        int drawX = graphMaxX - (historyCount - 1 - i); 
        if (drawX <= graphMinX) continue; 
        
        int y = graphTopY + graphHeight - pointRadius - (int)((data[i] - plotMin) / range * (graphHeight - 2*pointRadius));
        if (y < graphTopY + pointRadius) y = graphTopY + pointRadius;
        if (y > graphTopY + graphHeight - pointRadius) y = graphTopY + graphHeight - pointRadius;
        
        M5.Display.fillCircle(drawX, y, pointRadius, color);
    }
    
    if (drawLabels) {
        M5.Display.setTextColor(color);
        M5.Display.setFont(&fonts::Font0);
        
        // Max Label (Top Left + Offset)
        M5.Display.setTextDatum(top_right);
        M5.Display.drawString(String((int)plotMax), graphMinX - 2, graphTopY + labelOffsetY);

        // Mid Label (Middle Left + Offset)
        M5.Display.setTextDatum(middle_right);
        float midVal = (plotMax + plotMin) / 2.0;
        String midStr;
        if (midVal == (int)midVal) {
            midStr = String((int)midVal);
        } else {
            midStr = String(midVal, 1);
        }
        // Move UP by 7 pixels to balance the spacing between Top-Mid and Mid-Bot
        M5.Display.drawString(midStr, graphMinX - 2, graphTopY + (graphHeight / 2) + labelOffsetY - 7);

        // Min Label (Bottom Left + Offset)
        M5.Display.setTextDatum(bottom_right);
        M5.Display.drawString(String((int)plotMin), graphMinX - 2, graphTopY + graphHeight - (16 - labelOffsetY)); 
    }
}

void getMinMax(float* data, int type, float& outMin, float& outMax) {
    // Type 0=Temp, 1=Hum, 2=Press
    if (historyCount == 0) {
        // Defaults
        if (type == 0) { outMin = 0; outMax = 40; }
        if (type == 1) { outMin = 0; outMax = 100; }
        if (type == 2) { outMin = 950; outMax = 1050; }
        return;
    }

    float dMin = data[0];
    float dMax = data[0];
    for (int i=1; i<historyCount; i++) {
        if (data[i] < dMin) dMin = data[i];
        if (data[i] > dMax) dMax = data[i];
    }

    if (type == 0) { // Temp: Multiple of 5, Min range 5
        outMin = floor(dMin / 5.0) * 5.0;
        outMax = ceil(dMax / 5.0) * 5.0;
        if (outMax - outMin < 5.0) outMax = outMin + 5.0;
    } 
    else if (type == 1) { // Hum: Multiple of 5, 0-100
        outMin = floor(dMin / 5.0) * 5.0;
        if (outMin < 0) outMin = 0;
        outMax = ceil(dMax / 5.0) * 5.0;
        if (outMax > 100) outMax = 100;
        
        if (outMax - outMin < 5.0) {
            if (outMax + 5.0 <= 100) outMax += 5.0;
            else if (outMin - 5.0 >= 0) outMin -= 5.0;
        }
    } 
    else if (type == 2) { // Press: Multiple of 10, 600-1300
        outMin = floor(dMin / 10.0) * 10.0;
        if (outMin < 600) outMin = 600;
        outMax = ceil(dMax / 10.0) * 10.0;
        if (outMax > 1300) outMax = 1300;

        if (outMax - outMin < 10.0) {
            if (outMax + 10.0 <= 1300) outMax += 10.0;
            else if (outMin - 10.0 >= 600) outMin -= 10.0;
        }
    }
}

void drawGraph() {
    float minT, maxT, minH, maxH, minP, maxP;
    
    getMinMax(historyTemp, 0, minT, maxT);
    getMinMax(historyHum, 1, minH, maxH);
    getMinMax(historyPress, 2, minP, maxP);

    // Draw Axis Labels & Points
    
    drawSeries(historyTemp,  COLOR_TEMP,  minT, maxT, 1, 0, true);
    drawSeries(historyHum,   COLOR_HUM,   minH, maxH, 1, 8, true);
    drawSeries(historyPress, COLOR_PRESS, minP, maxP, 1, 16, true);

    // Vertical Axis Line
    M5.Display.drawFastVLine(graphMinX, graphTopY, graphHeight, WHITE);
}

void drawScreen() {
    M5.Display.fillRect(0, 0, screenWidth, graphTopY, BLACK);
    M5.Display.fillRect(0, graphTopY, screenWidth, graphHeight, BLACK);
    
    // Top Area: Current Values
    M5.Display.setTextDatum(top_center);
    M5.Display.setFont(&fonts::Font0); // Small font
    
    int cx = screenWidth / 2;
    int lineH = 10;
    int y = 5;

    // Temp
    M5.Display.setTextColor(COLOR_TEMP);
    M5.Display.drawString("Temp: " + String(currentData.temp, 1) + "C", cx, y);
    y += lineH;
    
    // Hum
    M5.Display.setTextColor(COLOR_HUM);
    M5.Display.drawString("Hum: " + String(currentData.hum, 1) + "%", cx, y);
    y += lineH;
    
    // Press
    M5.Display.setTextColor(COLOR_PRESS);
    M5.Display.drawString("Press: " + String(currentData.press, 0) + "hPa", cx, y);
    
    // Graph
    drawGraph();
}

void loop() {
    // WiFi Check
    if (WiFi.status() != WL_CONNECTED) {
        WiFiConnect();
        return; 
    }

    M5.update();

    if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
        lastMeasureTime = millis();
        // Measure
        if (sht3x.update()) {
            currentData.temp = sht3x.cTemp;
            currentData.hum = sht3x.humidity;
        }
        if (qmp.update()) {
            currentData.press = qmp.pressure / 100.0; // Pa -> hPa
        }
        
        Serial.printf("T:%.1f H:%.1f P:%.0f\n", currentData.temp, currentData.hum, currentData.press);
            
        drawScreen();
        updateAccumulator();
    }

    if (millis() - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = millis();
        
        if (accData.count > 0) {
            float avgT = accData.tempSum / accData.count;
            float avgH = accData.humSum / accData.count;
            float avgP = accData.pressSum / accData.count;

            Serial.println("--- Sending to Ambient ---");
            
            ambient.set(1, avgT);
            ambient.set(2, avgH);
            ambient.set(3, avgP);
            bool ret = ambient.send();
            Serial.printf("Ambient Send: %s\n", ret ? "OK" : "Fail");

            updateGraphHistory(avgT, avgH, avgP);
            drawScreen(); // Redraw with new history

            accData.tempSum = 0; accData.humSum = 0; accData.pressSum = 0;
            accData.count = 0;
        }
    }
}
