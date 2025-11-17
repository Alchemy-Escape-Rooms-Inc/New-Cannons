# ESP32 Pop Lock System

A complete system for controlling electronic pop locks (solenoid locks) via a web interface. Players enter a code on an iPad/tablet, and the corresponding lockbox opens automatically.

## Features

- **Web Interface**: Beautiful, touch-friendly interface optimized for iPad
- **Multiple Locks**: Control up to 6 locks (easily expandable)
- **Code Management**: Map access codes to specific locks
- **Auto-timeout**: Locks automatically deactivate after 2 seconds
- **MQTT Integration**: Remote monitoring and control
- **Master Code**: Special code to open all locks
- **Responsive Design**: Works on any device (iPad, phone, tablet, computer)

## Hardware Requirements

### Components
1. **ESP32** (any variant: ESP32, ESP32-S3, ESP32-C3, etc.)
2. **Electronic Pop Locks** (12V solenoid locks - 6 units)
3. **MOSFET modules** or relay modules (6 units)
   - Recommended: IRF520 MOSFET modules or 5V relay modules
4. **12V Power Supply** (for the locks)
   - Current capacity: At least 500mA per lock (3A minimum for 6 locks)
5. **Jumper Wires**
6. **Power distribution** (breadboard or terminal blocks)

### Wiring Diagram

```
ESP32 Pop Lock Wiring
=====================

For each lock:

ESP32 GPIO Pin → MOSFET/Relay Module → Pop Lock
                                      ↓
                                   12V Power Supply

Detailed:
---------
1. Connect ESP32 GPIO (e.g., GPIO 25) to MOSFET/Relay signal pin
2. Connect MOSFET/Relay GND to ESP32 GND
3. Connect MOSFET/Relay VCC to ESP32 3.3V (for logic)
4. Connect 12V+ to one terminal of pop lock
5. Connect other terminal of pop lock to MOSFET/Relay output
6. Connect 12V- (GND) to MOSFET/Relay ground rail

Power:
------
- ESP32: USB power (5V) OR external 5V supply
- Locks: External 12V DC power supply
- Common Ground: Connect ESP32 GND to 12V supply GND

Default GPIO Pin Assignment:
---------------------------
Lock 1: GPIO 25
Lock 2: GPIO 26
Lock 3: GPIO 27
Lock 4: GPIO 32
Lock 5: GPIO 33
Lock 6: GPIO 14
```

### MOSFET Module Connection (Per Lock)

```
┌─────────────┐
│   ESP32     │
│             │
│  GPIO 25 ───┼───→ Signal (MOSFET Gate)
│  GND ───────┼───→ GND
│  3.3V ──────┼───→ VCC (logic power)
└─────────────┘

┌──────────────────┐
│  MOSFET Module   │
│                  │
│  Signal ←────────┼─── ESP32 GPIO
│  VCC ←───────────┼─── ESP32 3.3V
│  GND ←───────────┼─── ESP32 GND
│  V+ ←────────────┼─── 12V+ (power supply)
│  GND ←───────────┼─── 12V- (power supply)
│  OUT ────────────┼───→ Pop Lock Terminal
└──────────────────┘

┌──────────────┐
│  Pop Lock    │
│              │
│  + ←─────────┼─── 12V+ (power supply)
│  - ←─────────┼─── MOSFET OUT
└──────────────┘
```

## Software Setup

### PlatformIO

1. **Update platformio.ini** to add WebServer library:

```ini
lib_deps =
  ...existing libraries...
  ESP32WebServer
```

2. **Upload the code**:
   - Open `src/PopLockSystem.cpp` in PlatformIO
   - Connect your ESP32 via USB
   - Click "Upload" or run: `pio run -t upload`

### Arduino IDE

1. **Install ESP32 board support**:
   - Go to: File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to: Tools → Board → Boards Manager
   - Search "ESP32" and install

2. **Install required libraries**:
   - Sketch → Include Library → Manage Libraries
   - Install: `PubSubClient` by Nick O'Leary

3. **Upload**:
   - Copy code from `PopLockSystem.cpp`
   - Select your board: Tools → Board → ESP32 Dev Module
   - Select port: Tools → Port → (your ESP32 port)
   - Click Upload

## Configuration

### WiFi Settings

Edit these lines in `PopLockSystem.cpp`:

```cpp
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASS = "YourPassword";
```

### Access Codes

Modify the `CODE_MAP` array:

```cpp
const CodeMapping CODE_MAP[] = {
  {"1234", 0, "Lockbox 1 - Red Box"},      // Code 1234 opens Lock 1
  {"5678", 1, "Lockbox 2 - Blue Box"},     // Code 5678 opens Lock 2
  {"9012", 2, "Lockbox 3 - Green Box"},    // etc...
  {"3456", 3, "Lockbox 4 - Yellow Box"},
  {"7890", 4, "Lockbox 5 - Purple Box"},
  {"2468", 5, "Lockbox 6 - Orange Box"},
  {"MASTER", -1, "Open All Locks"},        // Special master code
};
```

### GPIO Pins

Change lock pins if needed:

```cpp
const int LOCK_PINS[NUM_LOCKS] = {
  25,  // Lock 1
  26,  // Lock 2
  27,  // Lock 3
  32,  // Lock 4
  33,  // Lock 5
  14   // Lock 6
};
```

### Lock Timing

Adjust how long locks stay open:

```cpp
const int LOCK_ACTIVATION_TIME_MS = 2000;  // 2 seconds (2000ms)
```

## Usage

### 1. Upload and Power On

1. Upload code to ESP32
2. Open Serial Monitor (115200 baud)
3. Wait for WiFi connection
4. Note the IP address displayed

### 2. Access Web Interface

On your iPad/tablet:
1. Connect to the same WiFi network as ESP32
2. Open Safari/Chrome
3. Enter the IP address (e.g., `http://192.168.1.100`)
4. You'll see the lockbox access page

### 3. Enter Code

1. Use the on-screen keypad to enter a code
2. Press "UNLOCK"
3. If code is valid, the corresponding lock will activate for 2 seconds
4. Success/error message will be displayed

## Testing

### Test Individual Locks

Use the Serial Monitor commands or MQTT:

```cpp
// In setup() or loop(), add:
activateLock(0);  // Test lock 1
delay(3000);
activateLock(1);  // Test lock 2
```

### MQTT Testing

If you have an MQTT broker running:

```bash
# Unlock specific lock
mosquitto_pub -h 10.1.10.115 -t "PopLocks/command" -m "unlock:1"

# Unlock all locks
mosquitto_pub -h 10.1.10.115 -t "PopLocks/command" -m "unlock:all"

# Check status
mosquitto_sub -h 10.1.10.115 -t "PopLocks/#"
```

## Troubleshooting

### WiFi Won't Connect
- Check SSID and password
- Ensure ESP32 is within WiFi range
- Try 2.4GHz network (ESP32 doesn't support 5GHz)

### Lock Won't Activate
- Check wiring connections
- Verify GPIO pin numbers match your wiring
- Test with multimeter: GPIO should output 3.3V when active
- Check 12V power supply is connected
- Verify MOSFET/relay is working

### Web Page Won't Load
- Confirm ESP32 WiFi is connected (check Serial Monitor)
- Ensure iPad is on same network
- Try pinging the ESP32 IP address
- Check router firewall settings

### Lock Stays On Too Long/Short
- Adjust `LOCK_ACTIVATION_TIME_MS` value
- 1000 = 1 second, 2000 = 2 seconds, etc.

## Safety Considerations

1. **Power Supply**: Use appropriate current rating for your locks
2. **Heat**: MOSFETs may get warm under continuous use - add heatsinks if needed
3. **Voltage**: Ensure pop locks match your power supply voltage (typically 12V)
4. **Isolation**: Keep 12V power separate from ESP32 logic (only share GND)
5. **Testing**: Always test with one lock first before connecting all

## Customization Ideas

### Add More Locks
```cpp
const int NUM_LOCKS = 8;  // Increase number
const int LOCK_PINS[NUM_LOCKS] = {
  25, 26, 27, 32, 33, 14, 12, 13  // Add more pins
};
```

### Different Lock Timing Per Lock
```cpp
struct CodeMapping {
  const char* code;
  int lockIndex;
  const char* description;
  int activationTimeMs;  // Custom timing per code
};
```

### Add Authentication
```cpp
// Add password protection to web interface
// Add logging of all access attempts
// Add time-based code expiration
```

### Sound Effects
```cpp
// Add buzzer on GPIO for audio feedback
pinMode(23, OUTPUT);  // Buzzer pin
tone(23, 1000, 200);  // Success beep
```

## API Endpoints

### GET /
Returns the main web interface HTML page

### POST /unlock
Accepts form data: `code=1234`
Returns JSON:
```json
{
  "success": true,
  "message": "Lockbox 1 - Red Box opened!",
  "lock": 1
}
```

### GET /status
Returns system status in JSON:
```json
{
  "locks": [
    {"id": 1, "pin": 25, "active": false},
    {"id": 2, "pin": 26, "active": true},
    ...
  ]
}
```

## License

Free to use and modify for your escape room!

## Support

For questions or issues:
- Check Serial Monitor output for debugging
- Verify all connections match the wiring diagram
- Test components individually before full system integration
