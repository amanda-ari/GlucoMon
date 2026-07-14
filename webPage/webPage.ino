#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> // <-- New LCD Library
#include "MAX30105.h"

// --- Wi-Fi Credentials ---
const char* ssid     = "wifi name";
const char* password = "wifi pw";

MAX30105 particleSensor;
AsyncWebServer server(80);

// Initialize LCD. 0x27 is the standard address for most 16x2 I2C backpacks.
// If your screen stays blank, try changing 0x27 to 0x3F.
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// --- Global Sensor Variables ---
float latestGlucose = 0.0;
float latestRatio   = 0.0;
unsigned long lastSampleTime = 0;
bool lastFingerState = false; // Tracks state changes to prevent LCD flickering

// HTML Dashboard Layout Block (Kept from prior steps)
const char index_html[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BioMetrics // Glucose OS</title>
    <style>
        :root { --bg-color: #0d1117; --card-bg: #161b22; --border-color: #21262d; --text-main: #c9d1d9; --text-muted: #8b949e; --accent-glow: rgba(35, 134, 54, 0.15); }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif; background-color: var(--bg-color); color: var(--text-main); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; box-sizing: border-box; }
        .page-top-header { text-align: center; margin-bottom: 25px; }
        .page-top-header h2 { font-size: 20px; font-weight: 600; color: #ffffff; margin: 0 0 4px 0; letter-spacing: -0.5px; }
        .page-top-header p { font-size: 12px; color: var(--text-muted); margin: 0; text-transform: uppercase; letter-spacing: 1px; }
        .dashboard-card { width: 100%; max-width: 440px; background: var(--card-bg); border: 1px solid var(--border-color); border-radius: 20px; padding: 35px; box-shadow: 0 12px 40px rgba(0, 0, 0, 0.5); box-sizing: border-box; }
        .header { text-align: left; margin-bottom: 30px; border-bottom: 1px solid var(--border-color); padding-bottom: 20px; }
        .header h1 { font-size: 13px; font-weight: 700; text-transform: uppercase; letter-spacing: 2px; color: var(--text-muted); margin: 0 0 5px 0; }
        .status-badge { display: inline-flex; align-items: center; gap: 6px; font-size: 11px; font-weight: 600; text-transform: uppercase; background: rgba(139, 148, 158, 0.1); color: var(--text-muted); padding: 4px 10px; border-radius: 12px; letter-spacing: 0.5px; }
        .status-dot { width: 6px; height: 6px; background-color: var(--text-muted); border-radius: 50%; }
        .main-display { text-align: center; padding: 40px 0; margin: 20px 0; border-radius: 16px; background: radial-gradient(circle at center, var(--accent-glow) 0%, transparent 70%); transition: all 0.5s ease; }
        .value { font-size: 64px; font-weight: 300; letter-spacing: -2px; color: #ffffff; line-height: 1; }
        .unit { font-size: 14px; font-weight: 500; color: var(--text-muted); letter-spacing: 1px; text-transform: uppercase; margin-top: 10px; display: block; }
        .telemetry-grid { display: grid; grid-template-columns: 1fr; gap: 12px; margin-top: 25px; }
        .metric-row { display: flex; justify-content: space-between; align-items: center; background: rgba(255, 255, 255, 0.02); border: 1px solid var(--border-color); padding: 14px 18px; border-radius: 12px; font-size: 13px; }
        .metric-label { color: var(--text-muted); }
        .metric-value { font-family: monospace; font-weight: 600; color: #ffffff; }
        .disclaimer { margin-top: 35px; font-size: 10px; color: #484f58; text-align: center; line-height: 1.5; letter-spacing: 0.2px; }
    </style>
</head>
<body>
    <div class="page-top-header">
        <h2>GlucoMon Portal</h2>
        <p>Non-Invasive Glucose Level Monitoring System</p>
    </div>
    <div class="dashboard-card">
        <div class="header">
            <h1>Blood Glucose Estimation Level</h1>
            <div id="status-container" class="status-badge">
                <div id="status-pulse" class="status-dot"></div>
                <span id="status-text">Sensor Offline</span>
            </div>
        </div>
        <div class="main-display">
            <span id="glucose-val" class="value">--</span>
            <span class="unit">mg / dL</span>
        </div>
        <div class="telemetry-grid">
            <div class="metric-row">
                <span class="metric-label">Optical Component Ratio (IR/Red)</span>
                <span id="ratio-val" class="metric-value">0.0000</span>
            </div>
        </div>
        <div class="disclaimer">RESEARCH PROTOTYPE // NOT FOR MEDICAL DIAGNOSIS OR CLINICAL DECISIONS</div>
    </div>
    <script>
        const root = document.documentElement; const gVal = document.getElementById('glucose-val'); const rVal = document.getElementById('ratio-val'); const sText = document.getElementById('status-text'); const sDot = document.getElementById('status-pulse');
        setInterval(function() {
            fetch('/api/data').then(r => r.json()).then(d => {
                if(d.glucose > 0) {
                    gVal.innerText = d.glucose.toFixed(0); rVal.innerText = d.ratio.toFixed(4);
                    if (d.glucose < 70) { sText.innerText = "Hypoglycemia Alert"; sDot.style.backgroundColor = "#da3633"; root.style.setProperty('--accent-glow', 'rgba(218, 54, 51, 0.15)'); }
                    else if (d.glucose > 180) { sText.innerText = "Hyperglycemia Alert"; sDot.style.backgroundColor = "#f0883e"; root.style.setProperty('--accent-glow', 'rgba(240, 136, 62, 0.15)'); }
                    else { sText.innerText = "System Nominal"; sDot.style.backgroundColor = "#238636"; root.style.setProperty('--accent-glow', 'rgba(35, 134, 54, 0.15)'); }
                } else { gVal.innerText = "--"; sText.innerText = "Awaiting Placement"; sDot.style.backgroundColor = "#8b949e"; root.style.setProperty('--accent-glow', 'rgba(139, 148, 158, 0.05)'); }
            }).catch(e => { sText.innerText = "Hardware Offline"; sDot.style.backgroundColor = "#da3633"; });
        }, 1000);
    </script>
</body>
</html>
)rawhtml";

void setup() {
    Serial.begin(115200);
    Wire.begin(4, 5); 

    // --- Boot Up and Initialize LCD Screen ---
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Booting...");

    Serial.println("\nInitializing MAX30102...");
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        lcd.clear();
        lcd.print("Sensor Error!");
        while (1);
    }
    particleSensor.setup(0x1F, 4, 2, 400, 411, 4096); 

    // --- Manage Wi-Fi Connection Screen ---
    WiFi.begin(ssid, password);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi");
    
    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lcd.setCursor(15, 0);
        // Clean simple progress visualization
        if(dotCount % 2 == 0) lcd.print("."); else lcd.print(" ");
        dotCount++;
    }

    // --- Step 1: Output the IP Address Once Connected ---
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    
    // Hold IP on screen for 4 seconds so the user can easily read/type it
    delay(4000); 

    // --- Step 2: Establish the Initial "Waiting" Screen ---
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Glucose Monitor");
    lcd.setCursor(0, 1);
    lcd.print("Status: Waiting ");

    // Bind Web UI Endpoints
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });
    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{\"glucose\":" + String(latestGlucose) + ",\"ratio\":" + String(latestRatio, 4) + "}";
        request->send(200, "application/json", json);
    });
    server.begin();
}

void loop() {
    uint32_t irValue = particleSensor.getIR();
    uint32_t redValue = particleSensor.getRed();

    // Check finger presence (IR threshold boundary)
    if (irValue < 50000) {
        latestGlucose = 0.0;
        latestRatio = 0.0;

        // Reset to "Waiting" if finger was just removed
        if (lastFingerState == true) {
            lastFingerState = false;
            lcd.setCursor(0, 1);
            lcd.print("Status: Waiting "); // Trailing spaces clear out old digits cleanly
        }
    } else {
        // Collect metrics and estimate tracking algorithm
        if (millis() - lastSampleTime > 20) {
            lastSampleTime = millis();

            float currentRatio = (float)irValue / (float)redValue;
            latestRatio = (currentRatio * 0.1) + (latestRatio * 0.9);

            float slopeCoeff = -150.0; 
            float intercept  = 280.0;  
            latestGlucose = (latestRatio * slopeCoeff) + intercept;

            if (latestGlucose < 50.0)  latestGlucose = 50.0;
            if (latestGlucose > 250.0) latestGlucose = 250.0;

            // --- Step 3: Print Live Estimated Glucose Value When Finger is Engaged ---
            static unsigned long lastLcdUpdate = 0;
            // Limit LCD writing to every 500ms to eliminate visual display jitter
            if (millis() - lastLcdUpdate > 500) { 
                lastLcdUpdate = millis();
                lastFingerState = true;
                
                lcd.setCursor(0, 1);
                lcd.print("Glu: ");
                lcd.print((int)latestGlucose);
                lcd.print(" mg/dL   "); // Trailing spaces wipe clean old characters
            }
        }
    }
}
