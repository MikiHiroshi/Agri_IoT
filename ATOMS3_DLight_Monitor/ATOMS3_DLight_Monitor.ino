#include <M5Unified.h>
#include <M5_DLight.h>
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
#define COLOR_DLIGHT WHITE

// --- Globals ---
M5_DLight dlight;
Ambient ambient;
WiFiClient client;

// Display Dimensions
int screenWidth = 128;
int screenHeight = 128;

// Graph Margins
int graphMinX = 35;  // Increased for Y-axis labels
int graphMaxX = 126; // Inclusive
int graphTopY = 40;  // Moved up for taller graph
int graphHeight = 0;

// Sensor Data
struct SensorData {
    float lux;
    bool updated;
};

// Current instantaneous data
SensorData currentData = {0, false};

// Accumulator for 1-minute average
struct SensorAccumulator {
    float luxSum;
    int count;
} accData = {0, 0};

// History for Graph
float historyLux[MAX_HISTORY];
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

        // Small loop: Disconnect and retry every ~3 seconds (6 * 500ms)
        if (lpcnt > 6) {
            WiFi.disconnect(true, true);
            delay(1000);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            Serial.println("\nResetted WiFi, retrying...");
            lpcnt = 0;
            lpcnt2++;
        }

        // Big loop: Restart after ~3 cycles (approx 20-30 sec)
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
    
    // Re-init Ambient after WiFi connect
    client.setTimeout(3000); 
    ambient.begin(AMBIENT_CHANNEL_ID, AMBIENT_WRITE_KEY, &client);
    
    M5.Display.fillScreen(BLACK);
}

// --- Setup Functions ---

void setup() {
    // Initialization Sequence
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(500); 
    Serial.println("\nATOMS3 DLight Monitor Starting...");

    M5.Display.setRotation(0);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    Serial.printf("Screen: %dx%d\n", screenWidth, screenHeight);
    
    // Layout Calculation
    // Top 40px for Text (~30%), Bottom rest for graph
    graphTopY = 40;
    
    // Bottom limit: 125th dot (y=124).
    graphHeight = 125 - graphTopY;
    
    M5.Display.fillScreen(BLACK);

    // I2C Init (SDA=2, SCL=1 for ATOMS3)
    Wire.begin(2, 1);

    // DLight Init
    dlight.begin(&Wire);
    dlight.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
    Serial.println("DLight Initialized");

    // Robust WiFi Connect
    WiFiConnect();
    
    delay(1000);
    M5.Display.fillScreen(BLACK);
}

void updateAccumulator() {
    accData.luxSum += currentData.lux;
    accData.count++;
}

void updateGraphHistory(float lux) {
    if (historyCount < MAX_HISTORY) {
        historyLux[historyCount] = lux;
        historyCount++;
    } else {
        // Shift left
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            historyLux[i] = historyLux[i+1];
        }
        historyLux[MAX_HISTORY-1] = lux;
    }
}

void drawGraphPoints(float* data, uint16_t color) {
    // Ensure labels are drawn even if no history yet (show 0 and default max or empty)
    
    float plotMin = 0;
    float plotMax = 100; // Initial default

    if (historyCount > 0) {
        plotMax = data[0];
        // Auto-Scaling Logic
        for (int i = 1; i < historyCount; i++) {
            if (data[i] > plotMax) plotMax = data[i];
        }
    }

    // Fixed steps for cleaner scaling
    const float steps[] = {100, 300, 1000, 3000, 10000, 30000, 65000};
    float displayMax = 65000;
    for (float s : steps) {
        if (s >= plotMax) {
            displayMax = s;
            break;
        }
    }
    plotMax = displayMax;

    float range = plotMax - plotMin;
    if (range <= 0) range = 1.0; 
    
    // Draw Axis Labels (Left Side)
    M5.Display.setTextColor(WHITE);
    M5.Display.setFont(&fonts::Font0); // Small font (1)
    
    // Max Label (Top)
    M5.Display.setTextDatum(top_right); 
    M5.Display.drawString(String((int)plotMax), graphMinX - 4, graphTopY);

    // Mid Label (Middle)
    M5.Display.setTextDatum(middle_right);
    M5.Display.drawString(String((int)(plotMax / 2)), graphMinX - 4, graphTopY + graphHeight / 2);
    
    // Min Label (Bottom)
    M5.Display.setTextDatum(bottom_right);
    M5.Display.drawString("0", graphMinX - 4, graphTopY + graphHeight);

    // Draw Vertical Axis Line (Thinnest line)
    // Draw at graphMinX position, height of graph
    M5.Display.drawFastVLine(graphMinX, graphTopY, graphHeight, WHITE);

    if (historyCount == 0) return;

    int r = 1; // Point radius

    for (int i = 0; i < historyCount; i++) {
        // history[0] = Oldest, history[count-1] = Newest
        // We want Newest at Right (graphMaxX). Oldest at Left.
        // Left alignment starts at graphMinX + 1 to avoid overlapping axis line
        int drawX = graphMaxX - (historyCount - 1 - i); 
        
        if (drawX <= graphMinX) continue; 
        
        // Y calc within graph area
        int y = graphTopY + graphHeight - r - (int)((data[i] - plotMin) / range * (graphHeight - 2*r));
        if (y < graphTopY + r) y = graphTopY + r;
        if (y > graphTopY + graphHeight - r) y = graphTopY + graphHeight - r;
        
        M5.Display.fillCircle(drawX, y, r, color);
    }
}

void drawScreen() {
    // Clear Top & Bottom Areas
    M5.Display.fillRect(0, 0, screenWidth, graphTopY, BLACK);
    M5.Display.fillRect(0, graphTopY, screenWidth, graphHeight, BLACK);
    
    // Draw Text (Top)
    // Centered in the top 40px area (Y=20 center)
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COLOR_DLIGHT);
    
    // Large Font for Lux Value
    M5.Display.setFont(&fonts::Font4); 
    M5.Display.drawString(String((int)currentData.lux), screenWidth / 2, graphTopY / 2);
    
    // "lux" unit removed as requested

    // Draw Graph (Bottom)
    drawGraphPoints(historyLux, COLOR_DLIGHT);

    M5.Display.setTextDatum(top_left); // Reset
}

void loop() {
    // 1. WiFi Monitoring & Auto-Reconnect
    if (WiFi.status() != WL_CONNECTED) {
        WiFiConnect();
        // Return to prevent attempting other operations immediately
        return; 
    }

    M5.update();

    // Measurement Timer (1s)
    if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
        lastMeasureTime = millis();
        // Read DLight
        currentData.lux = dlight.getLUX();
        
        Serial.printf("DLight: Lux:%.1f\n", currentData.lux);
            
        drawScreen();
        updateAccumulator();
    }

    // Send Timer (1 min)
    if (millis() - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = millis();
        
        if (accData.count > 0) {
            float avgLux = accData.luxSum / accData.count;

            Serial.println("--- Sending to Ambient ---");
            Serial.printf("Avg Lux: %.1f\n", avgLux);
            
            // WiFi is guaranteed connected here due to check at start of loop
            ambient.set(1, avgLux);
            bool ret = ambient.send();
            Serial.printf("Ambient Send: %s\n", ret ? "OK" : "Fail");

            updateGraphHistory(avgLux);
            drawScreen();

            accData.luxSum = 0; accData.count = 0;
        }
    }
}
