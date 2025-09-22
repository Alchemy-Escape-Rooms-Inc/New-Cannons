#pragma once

#include <cstdint>
#include <cstddef>

// https://www.allegromicro.com/-/media/files/datasheets/als31300-datasheet.ashx

namespace ALS31300
{
    class Sensor
    {
        // See Customer Write Access
        static constexpr uint32_t customerAccessCode = 0x2C413534;
        static constexpr uint32_t customerAccessRegister = 0x35;

        bool write(uint8_t reg, uint32_t value);
        bool read(uint8_t reg, uint32_t& value);

        // I2C Interface
        typedef bool(*ReadCallback)(uint8_t address, uint8_t* sendPayload, size_t sendSize, uint8_t* receivePayload, size_t receiveSize);
        typedef bool (*WriteCallback)(uint8_t address, uint8_t* sendPayload, size_t sendSize);
        typedef bool(*RegisterCallback)(uint8_t address);
        typedef bool(*UnregisterCallback)(uint8_t address);
        typedef bool (*ChangeAddressCallback)(uint8_t oldAddress, uint8_t newAddress);

        static bool defaultReadCallback(uint8_t, uint8_t*, size_t, uint8_t*, size_t) { return false; }
        static bool defaultWriteCallback(uint8_t, uint8_t*, size_t) { return false; }
        static bool defaultRegisterCallback(uint8_t) { return false; }
        static bool defaultUnregisterCallback(uint8_t) { return false; }
        static bool defaultChangeAddressCallback(uint8_t, uint8_t) { return false; }

        static ReadCallback i2cRead;
        static WriteCallback i2cWrite;
        static RegisterCallback i2cRegister;
        static UnregisterCallback i2cUnregister;
        static ChangeAddressCallback i2cChangeAddress;

    public:
        static void setCallbacks(RegisterCallback registerCallback, UnregisterCallback unregisterCallback, ChangeAddressCallback changeAddressCallback, WriteCallback writeCallback, ReadCallback readCallback);

        Sensor(uint8_t address);
        ~Sensor();

        bool update();
        uint16_t getAngle();

        bool programAddress(uint8_t newAddress);

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        uint8_t address = 0;

    private:
        float avgAngle = 0.0f;

        float angleFromXY(float x, float y);
        void xyFromAngle(float angle, float& x, float& y);
    };
}