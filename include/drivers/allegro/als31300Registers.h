#pragma once
#include <cstdint>

namespace ALS31300
{
    struct Register0x02
    {
        union
        {
            struct
            {
                uint32_t raw             : 32;
            };
            struct
            {
                uint32_t customerEeprom  : 5;
                uint32_t intLatchEnable  : 1;
                uint32_t channelXEnable  : 1;
                uint32_t channelYEnable  : 1;
                uint32_t channelZEnable  : 1;
                uint32_t i2cThreshold    : 1;
                uint32_t slaveAddress    : 7;
                uint32_t disableSlaveAdc : 1;
                uint32_t i2cCrcEnable    : 1;
                uint32_t hallMode        : 2;
                uint32_t bwSelect        : 3;
                uint32_t reserved        : 8;
            };
        };
    };

    struct Register0x03
    {
        union
        {
            struct
            {
                uint32_t raw             : 32;
            };
            struct
            {
                uint32_t xIntThreshold   : 6;
                uint32_t yIntThreshold   : 6;
                uint32_t zIntThreshold   : 6;
                uint32_t xIntEnable      : 1;
                uint32_t yIntEnable      : 1;
                uint32_t zIntEnable      : 1;
                uint32_t intEepromEnable : 1;
                uint32_t intEepromStatus : 1;
                uint32_t intMode         : 1;
                uint32_t signedIntEnable : 1;
                uint32_t reserved        : 7;
            };
        };
    };

    struct Register0x0D
    {
        union
        {
            struct
            {
                uint32_t raw            : 32;
            };
            struct
            {
                uint32_t customerEeprom : 24;
                uint32_t reserved       : 8;
            };
        };
    };

    struct Register0x0E
    {
        union
        {
            struct
            {
                uint32_t raw            : 32;
            };
            struct
            {
                uint32_t customerEeprom : 24;
                uint32_t reserved       : 8;
            };
        };
    };

    struct Register0x0F
    {
        union
        {
            struct
            {
                uint32_t raw            : 32;
            };
            struct
            {
                uint32_t customerEeprom : 24;
                uint32_t reserved       : 8;
            };
        };
    };

    struct Register0x27
    {
        union
        {
            struct
            {
                uint32_t raw             : 32;
            };
            struct
            {
                uint32_t sleep           : 2;
                uint32_t i2cLoopMode     : 2;
                uint32_t lowPowerCounter : 3;
                uint32_t reserved        : 25;
            };
        };
    };

    struct Register0x28
    {
        union
        {
            struct
            {
                uint32_t raw             : 32;
            };
            struct
            {
                uint32_t temperatureMsbs : 6;
                uint32_t interrupt       : 1;
                uint32_t newData         : 1;
                uint32_t zAxisMsbs       : 8;
                uint32_t yAxisMsbs       : 8;
                uint32_t xAxisMsbs       : 8;
            };
        };
    };

    struct Register0x29
    {
        union
        {
            struct
            {
                uint32_t raw             : 32;
            };
            struct
            {
                uint32_t temperatureLsbs : 6;
                uint32_t hallStatus      : 2;
                uint32_t zAxisLsbs       : 4;
                uint32_t yAxisLsbs       : 4;
                uint32_t xAxisLsbs       : 4;
                uint32_t intWrite        : 1;
                uint32_t reserved        : 11;
            };
        };
    };
}