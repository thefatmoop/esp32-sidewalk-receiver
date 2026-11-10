#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sidewalk {

constexpr size_t MaxPsdu = 251;
constexpr size_t MaxAirBytes = MaxPsdu + 2;
constexpr uint8_t ChannelCount = 69;
constexpr uint32_t BeaconIntervalUs = 10080000;

enum class DecodeStatus {
    Ok,
    Truncated,
    InvalidHeader,
    UnsupportedLength,
    CrcMismatch
};

struct Frame {
    // Link-layer octets in the specification's representation, without FCS.
    uint8_t bytes[MaxPsdu] = {};
    uint16_t length = 0;
    uint16_t psduLength = 0;
    uint8_t fcsBytes = 0;
    bool whitening = false;
};

struct Beacon {
    uint8_t source[5] = {};
    uint8_t pan[5] = {};
    uint32_t seed = 0;
    uint16_t sequence = 0;
    uint8_t cloudConnection = 0;
    uint8_t loadIndicator = 0;
    bool roamingAllowed = false;
    bool synchronousOnly = false;
    uint8_t downlinkEirpDbm = 0;
    uint8_t uplinkAdjustmentDb = 0;
    uint8_t authenticationTagBytes = 0;
    bool hasSchedule = false;
    uint8_t csUlOffset = 0;
    uint8_t csUlPeriodicity = 0;
    uint8_t csDlOffset = 0;
    uint8_t csDlPeriodicity = 0;
};

enum class MessageStatus {
    Ok,
    NotMessage,
    UnsupportedVersion,
    InvalidHeader,
    Truncated,
    UnsupportedNetwork
};

struct Address {
    // Format 0 denotes a cloud service, 1 a device ID. The original size-coded
    // octets are retained. A complete 5-byte device ID can match a beacon ID.
    uint8_t format = 0;
    uint8_t length = 0;
    uint8_t bytes[5] = {};
};

struct Message {
    uint8_t frameType = 0;
    uint8_t protocolVersion = 0;
    bool linkValid = false;
    Address source;
    Address destination;
    uint32_t sequence = 0;
    uint8_t sequenceBytes = 0;
    bool epoch = false;
    bool ackRequested = false;
    bool networkPresent = false;
    uint16_t linkHeaderBytes = 0;
    uint16_t networkOffset = 0;
    uint16_t networkLength = 0;
    bool networkValid = false;
    bool encrypted = false;
    bool fragmented = false;
    uint8_t securityTopology = 0;
    bool extendedHeader = false;
    bool counterPresent = false;
    uint16_t counterOffset = 0;
    uint8_t counter[4] = {};
    uint16_t authenticationOffset = 0;
    uint8_t authenticationBytes = 0;
    // These ranges refer to Frame.bytes, and exclude security metadata/tag.
    uint16_t payloadOffset = 0;
    uint16_t payloadLength = 0;
    uint16_t ciphertextOffset = 0;
    uint16_t ciphertextLength = 0;
};

// Input is the raw, MSB-first octets immediately after the 0x904E SFD.
// PHR is not whitened. PSDU representation conversion is confined to decodeAir.
DecodeStatus inspectHeader(const uint8_t* air, size_t count, uint16_t& totalBytes);
DecodeStatus decodeAir(const uint8_t* air, size_t count, Frame& frame);
const char* statusName(DecodeStatus status);

// Accepts only the version-4, header-version-0 beacon layout. The authentication
// tag is structurally checked but no cryptographic authentication is performed.
bool parseBeacon(const Frame& frame, Beacon& beacon);
// The version-4 published link and network layouts are interpreted. Partial
// link metadata survives UnsupportedNetwork. Ciphertext ranges are populated
// only after validating a network header with its encryption bit enabled.
MessageStatus parseMessage(const Frame& frame, Message& message);
const char* messageStatusName(MessageStatus status);
const char* frameTypeName(uint8_t type);
bool sameGateway(const Beacon& first, const Beacon& second);
uint8_t beaconChannel(uint32_t seed, uint16_t sequence);
uint32_t channelFrequency(uint8_t channel);
uint8_t reverseByte(uint8_t byte);
// Independent published PHY vectors and synthetic beacon/message fixtures.
bool selfTest();

}  // namespace sidewalk
