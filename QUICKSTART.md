# Quick Start Guide - Pop Lock System

## 5-Minute Setup

### 1. Hardware (5 minutes)

```
For EACH lock:

ESP32 GPIO → MOSFET Signal
ESP32 GND → MOSFET GND
ESP32 3.3V → MOSFET VCC

12V+ → Pop Lock (+) terminal
Pop Lock (-) terminal → MOSFET OUT
12V GND → MOSFET GND (common ground with ESP32)
```

**Default Pins:**
- Lock 1: GPIO 25
- Lock 2: GPIO 26
- Lock 3: GPIO 27
- Lock 4: GPIO 32
- Lock 5: GPIO 33
- Lock 6: GPIO 14

### 2. Software (2 minutes)

**Option A: Arduino IDE**
1. Open `PopLockSystem.ino`
2. Edit WiFi credentials:
   ```cpp
   const char* WIFI_SSID = "YourWiFi";
   const char* WIFI_PASS = "YourPassword";
   ```
3. Upload to ESP32

**Option B: PlatformIO**
1. Use `src/PopLockSystem.cpp`
2. Edit WiFi credentials in the file
3. Run: `pio run -t upload`

### 3. Test (1 minute)

1. Open Serial Monitor (115200 baud)
2. Note the IP address (e.g., `192.168.1.50`)
3. On iPad, open browser: `http://192.168.1.50`
4. Enter code `1234` and click UNLOCK
5. Lock 1 should activate for 2 seconds

## Default Access Codes

| Code | Opens |
|------|-------|
| 1234 | Red Box (Lock 1) |
| 5678 | Blue Box (Lock 2) |
| 9012 | Green Box (Lock 3) |
| 3456 | Yellow Box (Lock 4) |
| 7890 | Purple Box (Lock 5) |
| 2468 | Orange Box (Lock 6) |
| MASTER | All Locks |

## Change Codes

Edit this section in the code:

```cpp
const CodeMap CODES[] = {
  {"1234", 0, "Red Box"},      // Lock 1
  {"5678", 1, "Blue Box"},     // Lock 2
  {"YOUR_CODE", 2, "Your Box"}, // Lock 3
  // Add your codes here
};
```

## Troubleshooting

**WiFi won't connect?**
- Check SSID and password spelling
- Use 2.4GHz network (not 5GHz)
- Move ESP32 closer to router

**Lock won't open?**
- Check wiring (especially common ground)
- Verify 12V power is connected
- Test with multimeter: GPIO should show 3.3V when active

**Web page won't load?**
- Confirm ESP32 IP address in Serial Monitor
- Ensure iPad is on same WiFi network
- Try refreshing the page

## Test Lock Directly

Add to `setup()` to test locks without web interface:

```cpp
void setup() {
  // ... existing code ...

  // Test lock 1
  Serial.println("Testing Lock 1...");
  activateLock(0);
  delay(3000);  // Wait 3 seconds

  Serial.println("System ready!");
}
```

## Shopping List

- ESP32 board ($5-10)
- 6× 12V solenoid locks ($3-5 each)
- 6× MOSFET modules (IRF520) ($1-2 each)
  - OR 6-channel relay module ($10)
- 12V power supply 3A+ ($10-15)
- Jumper wires ($5)
- Breadboard (optional) ($5)

**Total: ~$50-80**

## Safety Checklist

✓ Common ground between ESP32 and 12V supply
✓ Don't connect 12V directly to ESP32 pins
✓ Use MOSFET/relay for switching
✓ Power supply rated for total lock current
✓ Test one lock first before connecting all

## Support

See full documentation: `PopLockSystem_README.md`

Questions? Check Serial Monitor output for debugging info.
