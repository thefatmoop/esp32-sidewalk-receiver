#include "Sx1276Fsk.h"
#include "Sidewalk.h"

namespace {
constexpr uint8_t kChipSelect = 18;
constexpr uint8_t kReset = 16;
constexpr uint8_t kDio0 = 27;
constexpr uint8_t kFifo = 0x00;
constexpr uint8_t kOpMode = 0x01;
constexpr uint8_t kRxConfig = 0x0D;
constexpr uint8_t kIrqFlags1 = 0x3E;
constexpr uint8_t kIrqFlags2 = 0x3F;
constexpr uint8_t kFifoEmpty = 0x40;
constexpr uint8_t kFifoLevel = 0x20;
constexpr uint8_t kFifoOverrun = 0x10;
constexpr uint8_t kSyncMatch = 0x01;
constexpr uint8_t kPreambleDetect = 0x02;
constexpr uint8_t kRxConfigValue = 0x1E; // AFC + AGC, triggered by preamble.
constexpr uint32_t kMaximumPacketUs = 50000;
constexpr uint32_t kMaximumPreambleUs = 220000;
const SPISettings kSpiSettings(4000000, MSBFIRST, SPI_MODE0);
}

uint8_t Sx1276Fsk::readRegister(uint8_t address) {
    SPI.beginTransaction(kSpiSettings);
    digitalWrite(kChipSelect, LOW);
    SPI.transfer(address & 0x7F);
    uint8_t value = SPI.transfer(0);
    digitalWrite(kChipSelect, HIGH);
    SPI.endTransaction();
    return value;
}

void Sx1276Fsk::writeRegister(uint8_t address, uint8_t value) {
    SPI.beginTransaction(kSpiSettings);
    digitalWrite(kChipSelect, LOW);
    SPI.transfer(address | 0x80);
    SPI.transfer(value);
    digitalWrite(kChipSelect, HIGH);
    SPI.endTransaction();
}

bool Sx1276Fsk::begin(uint32_t initialHz) {
    pinMode(kChipSelect, OUTPUT);
    digitalWrite(kChipSelect, HIGH);
    pinMode(kDio0, INPUT);
    pinMode(kReset, OUTPUT);
    digitalWrite(kReset, LOW);
    delay(1);
    digitalWrite(kReset, HIGH);
    delay(10);
    SPI.begin(5, 19, 25, kChipSelect);
    version_ = readRegister(0x42);
    if (version_ != 0x12) {
        return false;
    }

    // LongRangeMode can only change in sleep. The only modes used here are
    // sleep, standby and FSK receive.
    writeRegister(kOpMode, readRegister(kOpMode) & 0xF8);
    delay(1);
    writeRegister(kOpMode, 0x00);
    writeRegister(kOpMode, 0x01);
    delay(1);

    writeRegister(0x02, 0x02); // 32 MHz / 640 = 50,000 bit/s.
    writeRegister(0x03, 0x80);
    writeRegister(0x5D, 0x00);
    writeRegister(0x04, 0x01); // +/-25 kHz nominal FSK deviation.
    writeRegister(0x05, 0x9A);
    writeRegister(0x0C, 0x23); // Maximum LNA gain and HF LNA boost.
    writeRegister(kRxConfig, kRxConfigValue);
    writeRegister(0x0E, 0x02); // RSSI averaging over eight samples.
    writeRegister(0x10, 0xE4); // -114 dBm threshold, RX trigger uses preamble.
    writeRegister(0x12, 0x02); // 125 kHz RX bandwidth.
    writeRegister(0x13, 0x11); // 166.7 kHz AFC acquisition bandwidth.
    writeRegister(0x1A, 0x01); // Clear previous AFC at each acquisition.
    writeRegister(0x1F, 0xCA); // Three preamble bytes, detector tolerance ten.
    writeRegister(0x20, 0x00); // Disable RSSI/preamble/sync hardware timeouts.
    writeRegister(0x21, 0x00);
    writeRegister(0x22, 0x00);
    writeRegister(0x25, 0x00);
    writeRegister(0x26, 0x04);
    writeRegister(0x27, 0x11); // Two-byte sync, software restarts reception.
    writeRegister(0x28, 0x90); // Uncoded SUN-FSK SFD, MSB first on the wire.
    writeRegister(0x29, 0x4E);
    writeRegister(0x30, 0x00); // Fixed packet, no CRC, whitening or address filter.
    // Keep the packet open while software consumes PHR + the declared PSDU.
    // The chip has an 11-bit fixed length and a streaming 64-byte FIFO.
    writeRegister(0x31, 0x47); // Packet mode, fixed hardware ceiling 2047 bytes.
    writeRegister(0x32, 0xFF);
    writeRegister(0x35, 0x02); // Retain a byte while streaming (SX1276 errata 3.1).
    writeRegister(0x36, 0x40); // Stop the autonomous sequencer.
    writeRegister(0x40, 0x00); // DIO0 PayloadReady, status polled through SPI.
    writeRegister(0x41, 0x30);

    ready_ = true;
    if (!tune(initialHz)) {
        ready_ = false;
        return false;
    }

    // Calibrate the HF receive chain at the operating band.
    writeRegister(kOpMode, 0x01);
    writeRegister(0x3B, readRegister(0x3B) | 0x40);
    uint32_t started = millis();
    while (readRegister(0x3B) & 0x20) {
        if (millis() - started > 30) {
            ready_ = false;
            return false;
        }
        delay(1);
    }
    restart();
    return true;
}

bool Sx1276Fsk::tune(uint32_t frequencyHz) {
    if (!ready_ || frequencyHz < 902000000 || frequencyHz > 928000000) {
        return false;
    }
    writeRegister(kOpMode, 0x01);
    uint32_t frf = (static_cast<uint64_t>(frequencyHz) * 524288 + 16000000) / 32000000;
    writeRegister(0x06, frf >> 16);
    writeRegister(0x07, frf >> 8);
    writeRegister(0x08, frf);
    frequencyHz_ = frequencyHz;
    restart();
    return true;
}

void Sx1276Fsk::restart() {
    writeRegister(kOpMode, 0x01);
    writeRegister(kIrqFlags1, 0x0B); // Clear RSSI, preamble and sync latches.
    writeRegister(kIrqFlags2, kFifoOverrun); // Write-one resets FIFO and its flags.
    writeRegister(kRxConfig, kRxConfigValue);
    collecting_ = false;
    preambleActive_ = false;
    expected_ = 0;
    pending_.length = 0;
    writeRegister(kOpMode, 0x05);
}

int16_t Sx1276Fsk::rssiDbm() {
    return -static_cast<int16_t>(readRegister(0x11)) / 2;
}

void Sx1276Fsk::startPacket() {
    collecting_ = true;
    packetStartedUs_ = micros();
    // SyncAddressMatch follows the last SFD bit. Polling adds up to one task
    // interval of uncertainty to this estimated SFD-start timestamp.
    pending_.syncUs = packetStartedUs_ - 320;
    pending_.frequencyHz = frequencyHz_;
    pending_.rssiDbm = rssiDbm();
    pending_.length = 0;
    expected_ = 0;
    ++counters.syncs;
}

bool Sx1276Fsk::poll(Packet& packet) {
    if (!ready_) {
        return false;
    }
    uint8_t irq1 = readRegister(kIrqFlags1);
    uint8_t irq2 = readRegister(kIrqFlags2);
    if (irq2 & kFifoOverrun) {
        ++counters.overflows;
        restart();
        return false;
    }
    if (!preambleActive_ && (irq1 & kPreambleDetect)) {
        preambleActive_ = true;
        preambleStartedUs_ = micros();
    }
    if (!collecting_ && ((irq1 & kSyncMatch) || !(irq2 & kFifoEmpty))) {
        startPacket();
    }
    if (!collecting_) {
        if (preambleActive_ && micros() - preambleStartedUs_ > kMaximumPreambleUs) {
            restart();
        }
        return false;
    }

    // Never empty the FIFO mid-packet. Once all declared bits have had time to
    // arrive, read the final one or two bytes. This avoids errata 3.1 and retains
    // the sync latch through streaming. Two milliseconds cover polling latency.
    while ((irq2 & kFifoLevel) ||
           (expected_ && micros() - packetStartedUs_ >= expected_ * 160U + 2000U &&
            !(irq2 & kFifoEmpty))) {
        pending_.bytes[pending_.length++] = readRegister(kFifo);
        if (pending_.length == 2) {
            if (sidewalk::inspectHeader(pending_.bytes, pending_.length, expected_) !=
                sidewalk::DecodeStatus::Ok) {
                ++counters.invalidHeaders;
                restart();
                return false;
            }
        }
        if (expected_ && pending_.length == expected_) {
            packet = pending_;
            restart();
            return true;
        }
        irq2 = readRegister(kIrqFlags2);
        if (irq2 & kFifoOverrun) {
            ++counters.overflows;
            restart();
            return false;
        }
    }
    if (static_cast<uint32_t>(micros() - packetStartedUs_) > kMaximumPacketUs) {
        ++counters.timeouts;
        restart();
    }
    return false;
}
