#pragma once

#include <Arduino.h>
#include <SPI.h>

// A single task owns SPI and polls this object at least once per millisecond.
// Packet bytes use the on-air MSB representation, including the two-byte PHR.
class Sx1276Fsk {
public:
    struct Packet {
        uint8_t bytes[257];
        uint16_t length;
        uint32_t syncUs;
        int16_t rssiDbm;
        uint32_t frequencyHz;
    };

    struct Counters {
        uint32_t syncs = 0;
        uint32_t overflows = 0;
        uint32_t invalidHeaders = 0;
        uint32_t timeouts = 0;
    } counters;

    bool begin(uint32_t initialHz = 903200000);
    bool tune(uint32_t frequencyHz);
    bool poll(Packet& packet);
    bool busy() const { return collecting_ || preambleActive_; }
    uint8_t version() const { return version_; }
    uint32_t frequencyHz() const { return frequencyHz_; }
    int16_t rssiDbm();

private:
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);
    void restart();
    void startPacket();

    uint8_t version_ = 0;
    bool ready_ = false;
    bool collecting_ = false;
    bool preambleActive_ = false;
    uint16_t expected_ = 0;
    uint32_t frequencyHz_ = 0;
    uint32_t preambleStartedUs_ = 0;
    uint32_t packetStartedUs_ = 0;
    Packet pending_{};
};
