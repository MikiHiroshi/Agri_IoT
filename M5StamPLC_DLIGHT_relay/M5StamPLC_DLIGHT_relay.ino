#include <M5StamPLC.h>
#include <M5_DLight.h>
#include <Ambient.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <sntp.h>
#include <Preferences.h>

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

// Conversion Constants
// 1 Lux ~= 0.0079 W/m2 (Sunlight approx)
// kW/m2 = Lux * 0.0079 / 1000
const double LUX_TO_KW_M2 = 0.0079 / 1000.0;

// --- Globals ---
M5_DLight dlight;
Ambient ambient;
WiFiClient client;
Preferences prefs;

// Screen Dimensions
int screenWidth = 240;
int screenHeight = 135;

// Graph Settings (Left 135px area)
int graphMinX = 35;
int graphMaxX = 126; 
int graphTopY = 40;
int graphHeight = 0; 

// Sensor Data
struct SensorData {
    float lux;
    bool updated;
};
SensorData currentData = {0, false};

// Solar Accumulation
double accumulatedEnergy = 0.0; // kW*h/m2
double accumulatedRelayTimeSec = 0.0; // Seconds
double lastTriggerEnergy = 0.0; // Energy value at last relay trigger
int currentDay = -1;

// Accumulator for Ambient (Lux)
struct SensorAccumulator {
    float luxSum;
    int count;
} accData = {0, 0};

// History (Lux)
float historyLux[MAX_HISTORY];
int historyCount = 0; 

// Timing
unsigned long lastMeasureTime = 0;
unsigned long lastSendTime = 0;
const unsigned long MEASURE_INTERVAL = 1000; // 1 second
const unsigned long SEND_INTERVAL = 60000;   // 1 minute

// Threshold & Relay
// Thresholds (BtnA) - Unit: kW*h/m2 (Delta increase)
float thresholds[] = {0.01, 0.03, 0.05, 0.1, 0.3, 0.5, 1.0, 3.0, 5.0};
int numThresholds = 9;
int thresholdIndex = 3; // Default 0.1
float currentThreshold = 0.1;

// Duration (BtnB) - Unit: Seconds
int durations[] = {5, 10, 15, 20, 30, 40, 50, 60};
int numDurations = 8;
int durationIndex = 0; // Default 5s
int currentDuration = 5;

// Relay State
bool relayState = false;
unsigned long relayStartTime = 0;

// --- WiFi & Time ---
void WiFiConnect() {
    int lpcnt = 0;
    int lpcnt2 = 0;

    M5StamPLC.Display.setTextDatum(middle_center);
    M5StamPLC.Display.setFont(&fonts::Font2);
    M5StamPLC.Display.fillScreen(BLACK);
    M5StamPLC.Display.drawString("Connecting WiFi...", screenWidth/2, screenHeight/2);
    
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lpcnt++;
        Serial.print(".");
        M5StamPLC.Display.fillCircle(screenWidth/2, screenHeight/2 + 20, 5, (lpcnt % 2 == 0) ? WHITE : BLACK);

        if (lpcnt > 6) {
            WiFi.disconnect(true, true);
            delay(1000);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            Serial.println("\nResetted WiFi, retrying...");
            lpcnt = 0;
            lpcnt2++;
        }

        if (lpcnt2 > 3) {
            ESP.restart();
        }
    }

    Serial.println("\nWiFi Connected");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    
    // Time Sync
    configTime(9 * 3600, 0, "ntp.nict.jp", "time.google.com");
    struct tm timeInfo;
    if (getLocalTime(&timeInfo)) {
        Serial.println("Time Synced");
        // Update Hardware RTC
        M5StamPLC.setRtcTime(&timeInfo);
        currentDay = timeInfo.tm_mday;
    } else {
        Serial.println("Time Sync Failed");
    }

    client.setTimeout(3000); 
    ambient.begin(AMBIENT_CHANNEL_ID, AMBIENT_WRITE_KEY, &client);
    
    M5StamPLC.Display.fillScreen(BLACK);
}

// --- Logic ---

void updateAccumulatorLux() {
    accData.luxSum += currentData.lux;
    accData.count++;
}

void updateSolarEnergy() {
    // Delta time in hours
    // Called every MEASURE_INTERVAL (1000ms)
    double dt_hours = (double)MEASURE_INTERVAL / 3600000.0;
    
    // Convert Lux to kW/m2
    double kw_m2 = currentData.lux * LUX_TO_KW_M2;
    
    // Add to accumulator
    accumulatedEnergy += kw_m2 * dt_hours;

    // Check for Daily Reset (Midnight)
    struct tm timeInfo;
    if (getLocalTime(&timeInfo)) {
        if (timeInfo.tm_mday != currentDay) {
            // New Day
            accumulatedEnergy = 0.0;
            accumulatedRelayTimeSec = 0.0;
            lastTriggerEnergy = 0.0; // Reset trigger baseline too? 
                                     // Usually yes for daily accumulation logic.
            currentDay = timeInfo.tm_mday;
            Serial.println("Daily Reset Executed");
        }
    }
}

void updateGraphHistory(float lux) {
    if (historyCount < MAX_HISTORY) {
        historyLux[historyCount] = lux;
        historyCount++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            historyLux[i] = historyLux[i+1];
        }
        historyLux[MAX_HISTORY-1] = lux;
    }
}

void drawGraphPoints(float* data, uint16_t color) {
    float plotMin = 0;
    float plotMax = 100;

    if (historyCount > 0) {
        plotMax = data[0];
        for (int i = 1; i < historyCount; i++) {
            if (data[i] > plotMax) plotMax = data[i];
        }
    }

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
    
    M5StamPLC.Display.setTextColor(WHITE);
    M5StamPLC.Display.setFont(&fonts::Font0);
    
    M5StamPLC.Display.setTextDatum(top_right); 
    M5StamPLC.Display.drawString(String((int)plotMax), graphMinX - 4, graphTopY);

    M5StamPLC.Display.setTextDatum(middle_right);
    M5StamPLC.Display.drawString(String((int)(plotMax / 2)), graphMinX - 4, graphTopY + graphHeight / 2);
    
    M5StamPLC.Display.setTextDatum(bottom_right);
    M5StamPLC.Display.drawString("0", graphMinX - 4, graphTopY + graphHeight);

    M5StamPLC.Display.drawFastVLine(graphMinX, graphTopY, graphHeight, WHITE);

    if (historyCount == 0) return;

    int r = 1;
    for (int i = 0; i < historyCount; i++) {
        int drawX = graphMaxX - (historyCount - 1 - i); 
        if (drawX <= graphMinX) continue; 
        
        int y = graphTopY + graphHeight - r - (int)((data[i] - plotMin) / range * (graphHeight - 2*r));
        if (y < graphTopY + r) y = graphTopY + r;
        if (y > graphTopY + graphHeight - r) y = graphTopY + graphHeight - r;
        
        M5StamPLC.Display.fillCircle(drawX, y, r, color);
    }
}

void drawScreen() {
    // 1. Clear Left Area (Graph)
    M5StamPLC.Display.setClipRect(0, 0, 135, screenHeight); 
    M5StamPLC.Display.fillRect(0, 0, 135, graphTopY, BLACK);
    M5StamPLC.Display.fillRect(0, graphTopY, 135, graphHeight, BLACK);
    
    // Draw Current Lux (Top Left)
    M5StamPLC.Display.setTextDatum(middle_center);
    M5StamPLC.Display.setTextColor(COLOR_DLIGHT);
    M5StamPLC.Display.setFont(&fonts::Font4); 
    M5StamPLC.Display.drawString(String((int)currentData.lux), 135 / 2, graphTopY / 2); 
    
    drawGraphPoints(historyLux, COLOR_DLIGHT);
    M5StamPLC.Display.clearClipRect();

    // 2. Refresh Right Area
    M5StamPLC.Display.fillRect(135, 0, 105, screenHeight, BLACK);
    int rightCenterX = 135 + (105 / 2);
    
    // 4 Items: Total(Acc), Limit(Th), Timer(Dur), Relay(State)
    // Adjusted for ~7px margins top/bottom and tighter label-value spacing
    // Item 1: Y=7, 16
    // Item 2: Y=36, 45
    // Item 3: Y=65, 74
    // Item 4: Y=94, 103 (Font4)

    M5StamPLC.Display.setTextColor(WHITE);
    
    // 1. Accumulated Energy
    M5StamPLC.Display.setTextDatum(top_center);
    M5StamPLC.Display.setFont(&fonts::Font0); 
    M5StamPLC.Display.drawString("Total kWh/m2", rightCenterX, 7);
    M5StamPLC.Display.setFont(&fonts::Font2); // Medium font
    M5StamPLC.Display.drawString(String(accumulatedEnergy, 4), rightCenterX, 16);

    // 2. Threshold (Limit)
    M5StamPLC.Display.setFont(&fonts::Font0);
    M5StamPLC.Display.setTextColor(YELLOW);
    M5StamPLC.Display.drawString("Limit (Delta)", rightCenterX, 36);
    M5StamPLC.Display.setFont(&fonts::Font2);
    M5StamPLC.Display.drawString(String(currentThreshold, 2), rightCenterX, 45);

    // 3. Timer Duration
    M5StamPLC.Display.setFont(&fonts::Font0);
    M5StamPLC.Display.setTextColor(CYAN);
    M5StamPLC.Display.drawString("Timer Set", rightCenterX, 65);
    M5StamPLC.Display.setFont(&fonts::Font2);
    M5StamPLC.Display.drawString(String(currentDuration) + "s", rightCenterX, 74);

    // 4. Relay State
    M5StamPLC.Display.setFont(&fonts::Font0);
    M5StamPLC.Display.setTextColor(WHITE);
    M5StamPLC.Display.drawString("Relay", rightCenterX, 94);
    
    M5StamPLC.Display.setFont(&fonts::Font4); // Large for State
    if (relayState) {
        M5StamPLC.Display.setTextColor(GREEN);
        M5StamPLC.Display.drawString("ON", rightCenterX, 103);
    } else {
        M5StamPLC.Display.setTextColor(RED);
        M5StamPLC.Display.drawString("OFF", rightCenterX, 103);
    }

    M5StamPLC.Display.setTextDatum(top_left); 
}

void checkRelay() {
    double diff = accumulatedEnergy - lastTriggerEnergy;

    // Trigger Logic: 
    // If accumulated energy Increased by 'currentThreshold' since last trigger
    if (diff >= currentThreshold) {
        if (!relayState) {
            relayState = true;
            relayStartTime = millis();
            lastTriggerEnergy = accumulatedEnergy; // Update baseline
            M5StamPLC.writePlcRelay(0, true);
            M5StamPLC.writePlcRelay(1, true);
            M5StamPLC.writePlcRelay(2, true);
            M5StamPLC.writePlcRelay(3, true);
            Serial.printf("Relay ON. Acc: %.4f, Last: %.4f\n", accumulatedEnergy, lastTriggerEnergy);
        }
    }
    
    // Timer Logic:
    if (relayState) {
        unsigned long elapsed = millis() - relayStartTime;
        if (elapsed >= (unsigned long)currentDuration * 1000) {
            relayState = false;
            M5StamPLC.writePlcRelay(0, false);
            M5StamPLC.writePlcRelay(1, false);
            M5StamPLC.writePlcRelay(2, false);
            M5StamPLC.writePlcRelay(3, false);
            Serial.println("Relay Timer OFF");
            // Note: We do NOT reset accumulatedEnergy here. 
            // We wait for the NEXT increment of 'currentThreshold'.
            // lastTriggerEnergy was already updated at ON event.
        }
    }
}

// --- Setup ---

void setup() {
    M5StamPLC.begin();
    
    M5StamPLC.Display.setRotation(1); 
    screenWidth = M5StamPLC.Display.width();
    screenHeight = M5StamPLC.Display.height();
    
    graphHeight = 125 - graphTopY;
    
    M5StamPLC.Display.fillScreen(BLACK);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("M5StamPLC Solar Relay Monitor");

    // I2C Init (Port A: uses Wire1)
    Wire1.begin(2, 1);
    
    dlight.begin(&Wire1);
    dlight.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);

    // Defaults & NVS Load
    prefs.begin("settings", false);
    thresholdIndex = prefs.getInt("thIdx", 3); // Default 0.1 (index 3)
    durationIndex = prefs.getInt("durIdx", 0); // Default 5s (index 0)
    
    // Bounds check
    if (thresholdIndex >= numThresholds) thresholdIndex = 0;
    if (durationIndex >= numDurations) durationIndex = 0;

    currentThreshold = thresholds[thresholdIndex];
    currentDuration = durations[durationIndex];
    M5StamPLC.writePlcRelay(0, false);
    M5StamPLC.writePlcRelay(1, false);
    M5StamPLC.writePlcRelay(2, false);
    M5StamPLC.writePlcRelay(3, false);

    WiFiConnect();
    
    // Set initial Day
    struct tm timeInfo;
    if (getLocalTime(&timeInfo)) {
        currentDay = timeInfo.tm_mday;
    }

    delay(1000);
    M5StamPLC.Display.fillScreen(BLACK);
    drawScreen(); 
}

void loop() {
    M5StamPLC.update();
    bool needRedraw = false;

    // BtnA: Threshold
    if (M5StamPLC.BtnA.wasPressed()) { 
        thresholdIndex++;
        if (thresholdIndex >= numThresholds) {
            thresholdIndex = 0;
        }
        currentThreshold = thresholds[thresholdIndex];
        prefs.putInt("thIdx", thresholdIndex); // Save
        needRedraw = true;
    }

    // BtnB: Duration
    if (M5StamPLC.BtnB.wasPressed()) { 
        durationIndex++;
        if (durationIndex >= numDurations) {
            durationIndex = 0;
        }
        currentDuration = durations[durationIndex];
        prefs.putInt("durIdx", durationIndex); // Save
        needRedraw = true;
    }

    // BtnC: Manual Relay Toggle
    if (M5StamPLC.BtnC.wasPressed()) {
        relayState = !relayState;
        if (relayState) {
            // Turned ON
            relayStartTime = millis();
            lastTriggerEnergy = accumulatedEnergy; // Reset trigger baseline on manual ON too? 
                                                   // Maybe safer to synchronize baseline to avoid immediate re-trigger
            M5StamPLC.writePlcRelay(0, true);
            M5StamPLC.writePlcRelay(1, true);
            M5StamPLC.writePlcRelay(2, true);
            M5StamPLC.writePlcRelay(3, true);
            Serial.println("Manual Relay ON");
        } else {
            // Turned OFF
            M5StamPLC.writePlcRelay(0, false);
            M5StamPLC.writePlcRelay(1, false);
            M5StamPLC.writePlcRelay(2, false);
            M5StamPLC.writePlcRelay(3, false);
            Serial.println("Manual Relay OFF");
        }
        needRedraw = true;
    }

    if (needRedraw) {
        drawScreen();
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFiConnect();
        return;
    }

    // Measure (1s)
    if (millis() - lastMeasureTime >= MEASURE_INTERVAL) {
        lastMeasureTime = millis();
        
        // Retry logic for reliability
        float val = dlight.getLUX();
        if (val == 0) {
            for(int i=0; i<3; i++) {
                delay(50);
                val = dlight.getLUX();
                if (val > 0) break;
            }
        }
        currentData.lux = val;
        
        updateSolarEnergy();
        checkRelay();
        
        // Accumulate Relay Time (Approx 1s per loop if ON)
        if (relayState) {
            accumulatedRelayTimeSec += (MEASURE_INTERVAL / 1000.0);
        }

        drawScreen();
        updateAccumulatorLux();
        
        Serial.printf("Lux:%.1f, Acc:%.5f, RelayTime:%.0fs, TrgDiff:%.5f\n", 
            currentData.lux, accumulatedEnergy, accumulatedRelayTimeSec, accumulatedEnergy - lastTriggerEnergy);
    }

    // Send (60s)
    if (millis() - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = millis();
        
        if (accData.count > 0) {
            float avgLux = accData.luxSum / accData.count;
            Serial.println("--- Sending to Ambient ---");
            
            // Channel 1: Lux
            ambient.set(1, avgLux);
            // Channel 2: Accumulated Energy (kWh/m2)
            ambient.set(2, accumulatedEnergy);
            // Channel 3: Accumulated Relay Time (sec)
            ambient.set(3, accumulatedRelayTimeSec);
            
            bool ret = ambient.send();
            Serial.printf("Ambient Send: %s\n", ret ? "OK" : "Fail");

            updateGraphHistory(avgLux);
            drawScreen(); 

            accData.luxSum = 0; accData.count = 0;
        }
    }
}
