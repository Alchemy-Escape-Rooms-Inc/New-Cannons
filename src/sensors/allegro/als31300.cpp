
#include <cstdio>
#include <cmath>

#include "boardkit.hpp"

namespace allegro_constants {
  constexpr float kPi       = 3.14159265358979323846f;
  constexpr float kDegToRad = kPi / 180.0f;
  constexpr float kRadToDeg = 180.0f / kPi;
}

namespace ALS31300
{
    Sensor::ReadCallback Sensor::i2cRead = defaultReadCallback;
    Sensor::WriteCallback Sensor::i2cWrite = defaultWriteCallback;
    Sensor::RegisterCallback Sensor::i2cRegister = defaultRegisterCallback;
    Sensor::UnregisterCallback Sensor::i2cUnregister = defaultUnregisterCallback;
    Sensor::ChangeAddressCallback Sensor::i2cChangeAddress = defaultChangeAddressCallback;

    void Sensor::setCallbacks(RegisterCallback registerCallback, UnregisterCallback unregisterCallback, ChangeAddressCallback changeAddressCallback, WriteCallback writeCallback, ReadCallback readCallback)
    {
        i2cWrite = writeCallback;
        i2cRead = readCallback;
        i2cRegister = registerCallback;
        i2cUnregister = unregisterCallback;
        i2cChangeAddress = changeAddressCallback;
    }

    Sensor::Sensor(uint8_t address) : address(address & 0x7F)
    {
        i2cRegister(address);
    }

    Sensor::~Sensor()
    {
        i2cUnregister(address);
    }

    bool Sensor::update()
    {
        uint16_t newX, newY, newZ;

        uint32_t readData;
        if (!read(0x28, readData)) return false;
        Register0x28 reg28{readData};

        newX = reg28.xAxisMsbs << 8;
        newY = reg28.yAxisMsbs << 8;
        newZ = reg28.zAxisMsbs << 8;

        if (!read(0x29, readData)) return false;
        Register0x29 reg29{readData};

        newX |= reg29.xAxisLsbs;
        newY |= reg29.yAxisLsbs;
        newZ |= reg29.zAxisLsbs;

        const float filterIntensity = 32.0f;
        x = (float((int16_t) newX) + x * (filterIntensity - 1)) / filterIntensity;
        y = (float((int16_t) newY) + y * (filterIntensity - 1)) / filterIntensity;
        z = (float((int16_t) newZ) + z * (filterIntensity - 1)) / filterIntensity;

        return true;
    }

    bool Sensor::programAddress(uint8_t newAddress)
    {
        newAddress &= 0x7F;
        
        // Enter Customer Access Mode to enable register writes
        if (!write(customerAccessRegister, customerAccessCode)) return false;

        // Read register
        uint32_t readData;
        if (!read(0x02, readData)) return false;

        // Update address
        Register0x02 registerData = {readData};
        registerData.slaveAddress = newAddress;

        // Write new address
        if (!write(0x02, registerData.raw)) return false;

        printf("Address programming successful! Power cycle device to check.\n");
        
        return true;
    }

    bool Sensor::write(uint8_t reg, uint32_t value)
    {
        uint8_t sendPayload[5] =
        {
            reg,
            uint8_t(value >> 24 & 0xFF),
            uint8_t(value >> 16 & 0xFF),
            uint8_t(value >>  8 & 0xFF),
            uint8_t(value >>  0 & 0xFF)
        };

        return i2cWrite(address, sendPayload, 5);
    }

    bool Sensor::read(uint8_t reg, uint32_t& value)
    {
        uint8_t sendPayload = reg;
        uint8_t receivePayload[4];

        if (!i2cRead(address, &sendPayload, 1, receivePayload, 4)) return false;

        value = receivePayload[0] << 24 |
                receivePayload[1] << 16 |
                receivePayload[2] <<  8 |
                receivePayload[3];

        return true;
    }

    uint16_t Sensor::getAngle()
    {
        // TODO, normalize x and y to skip multiple conversions
        float currentAngle = angleFromXY(x, y);
        float currentX, currentY;
        xyFromAngle(currentAngle, currentX, currentY);

        float avgX, avgY;
        xyFromAngle(avgAngle, avgX, avgY);

        const float filterIntensity = 10.0f;
        avgX = (currentX + avgX * (filterIntensity - 1)) / filterIntensity;
        avgY = (currentY + avgY * (filterIntensity - 1)) / filterIntensity;

        avgAngle = angleFromXY(avgX, avgY);
        
        return avgAngle + 0.5f; // Round angle before casting to uint16_t
    }

    float Sensor::angleFromXY(float x, float y)
    {
        float angle = atan2f(y, x) * allegro_constants::kRadToDeg;
        if (angle < 0) angle += 360.0f;
        
        return angle;
    }

    void Sensor::xyFromAngle(float angle, float& x, float& y)
    {
        x = cos(angle * allegro_constants::kDegToRad);
        y = sin(angle * allegro_constants::kDegToRad);
    }
}