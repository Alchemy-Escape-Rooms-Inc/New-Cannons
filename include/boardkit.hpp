
#pragma once


#include "board/pins.h"
#include "hal/gpio.h"                   // the header above
#include "hal/i2c.h"
#include "net/INetClient.h"
#include "protocols/mqtt/MqttClient.h"
#include "protocols/mqtt/MqttTopic.h"
#include "hal/input/DebouncedButton.h"
#include "features/telemetry/TelemetryPublisher.h"
#include "hardware/controller.h"


// Arduino specific Headers 
#include "protocols/mqtt/adapters/Arduino/ArduinoPubSubClientAdapter.h"
#include "net/adapters/Arduino/ArduinoEthClientAdapter.h"

// Individual Controller Specific Headers
// #include "sensors/allegro/als31300.h"  --- IGNORE

#include "drivers/allegro/als31300.h"
#include "drivers/allegro/als31300Registers.h"
