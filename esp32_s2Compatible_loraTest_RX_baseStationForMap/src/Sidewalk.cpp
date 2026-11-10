#include "Sidewalk.h"

#include <string.h>

namespace sidewalk {

uint8_t reverseByte(uint8_t byte) {
    byte = static_cast<uint8_t>((byte >> 4) | (byte << 4));
    byte = static_cast<uint8_t>(((byte & 0xCC) >> 2) | ((byte & 0x33) << 2));
    return static_cast<uint8_t>(((byte & 0xAA) >> 1) | ((byte & 0x55) << 1));
}

DecodeStatus inspectHeader(const uint8_t* air, size_t count, uint16_t& totalBytes) {
    totalBytes = 0;
    if (air == nullptr || count < 2) return DecodeStatus::Truncated;
    if ((air[0] & 0xE0) != 0) return DecodeStatus::InvalidHeader;
    const uint16_t length = static_cast<uint16_t>(((air[0] & 7) << 8) | air[1]);
    const uint8_t fcsBytes = (air[0] & 0x10) ? 2 : 4;
    if (length <= fcsBytes || length > MaxPsdu) return DecodeStatus::UnsupportedLength;
    totalBytes = length + 2;
    return DecodeStatus::Ok;
}

static uint16_t crc16(const uint8_t* bytes, size_t count) {
    uint16_t crc = 0;
    for (size_t index = 0; index < count; ++index) {
        crc ^= bytes[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint16_t>((crc >> 1) ^ ((crc & 1) ? 0x8408 : 0));
        }
    }
    return crc;
}

static uint32_t crc32(const uint8_t* bytes, size_t count) {
    uint32_t crc = 0xFFFFFFFF;
    // SUN-FSK CRC32 includes untransmitted zero padding for bodies below 4 B.
    const size_t paddedCount = count < 4 ? 4 : count;
    for (size_t index = 0; index < paddedCount; ++index) {
        crc ^= index < count ? bytes[index] : 0;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

DecodeStatus decodeAir(const uint8_t* air, size_t count, Frame& frame) {
    frame = Frame{};
    uint16_t total = 0;
    const DecodeStatus headerStatus = inspectHeader(air, count, total);
    if (headerStatus != DecodeStatus::Ok) return headerStatus;
    if (count < total) return DecodeStatus::Truncated;

    const uint16_t psduLength = total - 2;
    const uint8_t fcsBytes = (air[0] & 0x10) ? 2 : 4;
    const bool whitening = (air[0] & 8) != 0;
    uint8_t canonical[MaxPsdu];
    uint16_t pn9 = 0x1FF;
    for (uint16_t index = 0; index < psduLength; ++index) {
        uint8_t byte = reverseByte(air[index + 2]);
        if (whitening) {
            uint8_t mask = 0;
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const uint16_t feedback = (pn9 ^ (pn9 >> 5)) & 1;
                pn9 = static_cast<uint16_t>((pn9 >> 1) | (feedback << 8));
                mask |= static_cast<uint8_t>(feedback << bit);
            }
            byte ^= mask;
        }
        canonical[index] = byte;
    }

    const uint16_t payloadLength = psduLength - fcsBytes;
    uint32_t expected;
    if (fcsBytes == 2) expected = crc16(canonical, payloadLength);
    else expected = crc32(canonical, payloadLength);
    for (uint8_t index = 0; index < fcsBytes; ++index) {
        if (canonical[payloadLength + index] != static_cast<uint8_t>(expected >> (index * 8))) {
            return DecodeStatus::CrcMismatch;
        }
    }

    // The CRC domain uses reflected PHY bytes. Sidewalk's link octet diagrams
    // use the inverse bit representation, including all multibyte field octets.
    for (uint16_t index = 0; index < payloadLength; ++index) {
        frame.bytes[index] = reverseByte(canonical[index]);
    }
    frame.length = payloadLength;
    frame.psduLength = psduLength;
    frame.fcsBytes = fcsBytes;
    frame.whitening = whitening;
    return DecodeStatus::Ok;
}

const char* statusName(DecodeStatus status) {
    switch (status) {
        case DecodeStatus::Ok: return "crc_ok";
        case DecodeStatus::Truncated: return "truncated";
        case DecodeStatus::InvalidHeader: return "invalid_phr";
        case DecodeStatus::UnsupportedLength: return "unsupported_length";
        case DecodeStatus::CrcMismatch: return "crc_failed";
    }
    return "unknown";
}

bool parseBeacon(const Frame& frame, Beacon& beacon) {
    beacon = Beacon{};
    const uint8_t* bytes = frame.bytes;
    if (frame.length < 25 || frame.length > MaxPsdu || bytes[0] != 0x04 || bytes[1] != 0) return false;
    // Reserved connection and security bits must be clear for this layout.
    if ((bytes[12] & 3) != 0 || (bytes[20] & 0xF8) != 0) return false;
    const size_t tagBytes = ((bytes[20] & 3) + 1) * 4;
    const size_t dataEnd = frame.length - tagBytes;
    if (dataEnd < 21) return false;
    const bool dataIncluded = (bytes[20] & 4) != 0;
    if (dataIncluded != (dataEnd > 21)) return false;

    Beacon parsed;
    memcpy(parsed.source, bytes + 2, 5);
    memcpy(parsed.pan, bytes + 7, 5);
    parsed.roamingAllowed = (bytes[12] & 0x80) != 0;
    parsed.cloudConnection = (bytes[12] >> 5) & 3;
    parsed.loadIndicator = (bytes[12] >> 3) & 3;
    parsed.synchronousOnly = (bytes[12] & 4) != 0;
    parsed.downlinkEirpDbm = (bytes[13] >> 4) * 4;
    parsed.uplinkAdjustmentDb = (bytes[13] & 15) * 4;
    parsed.seed = (static_cast<uint32_t>(bytes[14]) << 24) |
                  (static_cast<uint32_t>(bytes[15]) << 16) |
                  (static_cast<uint32_t>(bytes[16]) << 8) | bytes[17];
    parsed.sequence = static_cast<uint16_t>((bytes[18] << 8) | bytes[19]);
    parsed.authenticationTagBytes = static_cast<uint8_t>(tagBytes);

    size_t position = 21;
    while (position < dataEnd) {
        const uint8_t control = bytes[position++];
        const uint8_t lengthCode = control >> 6;
        const uint8_t type = control & 0x3F;
        size_t valueLength = static_cast<size_t>(1) << lengthCode;
        if (lengthCode == 3) {
            if (position >= dataEnd) return false;
            valueLength = bytes[position++];
        }
        if (valueLength == 0 || valueLength > dataEnd - position) return false;
        if (type == 1) {
            if (valueLength != 4 || parsed.hasSchedule) return false;
            parsed.hasSchedule = true;
            parsed.csUlOffset = bytes[position];
            parsed.csUlPeriodicity = bytes[position + 1];
            parsed.csDlOffset = bytes[position + 2];
            parsed.csDlPeriodicity = bytes[position + 3];
        }
        position += valueLength;
    }
    beacon = parsed;
    return true;
}

static MessageStatus readAddress(const Frame& frame, size_t& position, Address& address) {
    if (position >= frame.length) return MessageStatus::Truncated;
    const uint8_t length = frame.bytes[position] >> 5;
    if (length == 0 || length > sizeof(address.bytes)) return MessageStatus::InvalidHeader;
    if (length > frame.length - position) return MessageStatus::Truncated;
    address.length = length;
    memcpy(address.bytes, frame.bytes + position, length);
    position += length;
    return MessageStatus::Ok;
}

MessageStatus parseMessage(const Frame& frame, Message& message) {
    message = Message{};
    if (frame.length == 0) return MessageStatus::Truncated;
    if (frame.length > MaxPsdu) return MessageStatus::InvalidHeader;
    const uint8_t* bytes = frame.bytes;
    message.frameType = bytes[0] >> 5;
    message.protocolVersion = bytes[0] & 15;
    if (message.frameType < 1 || message.frameType > 3) return MessageStatus::NotMessage;
    if (message.protocolVersion != 4) return MessageStatus::UnsupportedVersion;
    if (frame.length < 2) return MessageStatus::Truncated;

    message.epoch = (bytes[0] & 0x10) != 0;
    message.source.format = bytes[1] >> 6;
    message.destination.format = (bytes[1] >> 4) & 3;
    message.networkPresent = (bytes[1] & 8) != 0;
    message.ackRequested = (bytes[1] & 4) != 0;
    if ((bytes[1] & 3) != 0 || message.source.format > 1 || message.destination.format > 1) {
        return MessageStatus::InvalidHeader;
    }
    // Epoch is defined only for asynchronous-uplink Data. ACK and Network Data
    // reserve this bit. ACK is terminated at the link layer and has no payload.
    const bool ack = message.frameType == 3;
    if ((message.epoch && message.frameType != 1) ||
        (ack && (message.networkPresent || message.ackRequested)) ||
        (!ack && !message.networkPresent)) return MessageStatus::InvalidHeader;

    size_t position = 2;
    MessageStatus status = readAddress(frame, position, message.source);
    if (status != MessageStatus::Ok) return status;
    status = readAddress(frame, position, message.destination);
    if (status != MessageStatus::Ok) return status;
    if (position >= frame.length) return MessageStatus::Truncated;
    const uint8_t sequenceBytes = bytes[position] >> 6;
    if (sequenceBytes == 0 || sequenceBytes > 3) return MessageStatus::InvalidHeader;
    if (sequenceBytes > frame.length - position) return MessageStatus::Truncated;
    message.sequenceBytes = sequenceBytes;
    message.sequence = bytes[position++] & 0x3F;
    for (uint8_t index = 1; index < sequenceBytes; ++index) {
        message.sequence = (message.sequence << 8) | bytes[position++];
    }
    message.linkHeaderBytes = static_cast<uint16_t>(position);
    message.networkOffset = static_cast<uint16_t>(position);
    message.networkLength = static_cast<uint16_t>(frame.length - position);
    if (ack) {
        if (position != frame.length) return MessageStatus::InvalidHeader;
        message.linkValid = true;
        return MessageStatus::Ok;
    }
    message.linkValid = true;
    if (position >= frame.length) return MessageStatus::Truncated;

    const uint8_t networkControl = bytes[position++];
    message.fragmented = (networkControl & 0x80) != 0;
    message.securityTopology = (networkControl >> 1) & 1;
    message.extendedHeader = (networkControl & 1) != 0;
    const uint8_t authenticationCode = (networkControl >> 2) & 7;
    static const uint8_t authenticationLengths[] = {0, 2, 4, 8, 12, 16};
    // Only the published unfragmented Presentation/WAN layout is supported.
    // Preserve the link header, but do not assign a guessed ciphertext range
    // to reserved or future network configurations.
    if (message.fragmented || !(networkControl & 0x40) || message.securityTopology != 1 ||
        authenticationCode >= sizeof(authenticationLengths) ||
        (message.extendedHeader && authenticationCode == 0)) return MessageStatus::UnsupportedNetwork;

    bool counterPresent = false;
    if (message.extendedHeader) {
        if (position >= frame.length) return MessageStatus::Truncated;
        const uint8_t securityReference = bytes[position++];
        if ((securityReference & 0x7F) != 0) return MessageStatus::UnsupportedNetwork;
        counterPresent = (securityReference & 0x80) != 0;
    }
    const uint8_t authenticationBytes = authenticationLengths[authenticationCode];
    if (counterPresent && authenticationBytes > 12) return MessageStatus::UnsupportedNetwork;
    if (counterPresent) {
        if (frame.length - position < sizeof(message.counter)) return MessageStatus::Truncated;
        message.counterOffset = static_cast<uint16_t>(position);
        memcpy(message.counter, bytes + position, sizeof(message.counter));
        position += sizeof(message.counter);
    }
    // Authentication code precedes the presentation payload in all three
    // published network formats. It is not part of the encrypted byte range.
    if (authenticationBytes >= frame.length - position) return MessageStatus::Truncated;
    const uint16_t authenticationOffset = static_cast<uint16_t>(position);
    position += authenticationBytes;
    const size_t payloadLength = frame.length - position;
    if (payloadLength > 221) return MessageStatus::UnsupportedNetwork;

    message.authenticationOffset = authenticationOffset;
    message.authenticationBytes = authenticationBytes;
    message.counterPresent = counterPresent;
    message.payloadOffset = static_cast<uint16_t>(position);
    message.payloadLength = static_cast<uint16_t>(payloadLength);
    message.encrypted = (networkControl & 0x20) != 0;
    message.networkValid = true;
    if (message.encrypted) {
        message.ciphertextOffset = message.payloadOffset;
        message.ciphertextLength = message.payloadLength;
    }
    return MessageStatus::Ok;
}

const char* messageStatusName(MessageStatus status) {
    switch (status) {
        case MessageStatus::Ok: return "parsed";
        case MessageStatus::NotMessage: return "not_message";
        case MessageStatus::UnsupportedVersion: return "unsupported_version";
        case MessageStatus::InvalidHeader: return "invalid_link_header";
        case MessageStatus::Truncated: return "truncated_message";
        case MessageStatus::UnsupportedNetwork: return "unsupported_network";
    }
    return "unknown";
}

const char* frameTypeName(uint8_t type) {
    switch (type) {
        case 0: return "beacon";
        case 1: return "data";
        case 2: return "network_data";
        case 3: return "ack";
        default: return "unknown";
    }
}

bool sameGateway(const Beacon& first, const Beacon& second) {
    return memcmp(first.source, second.source, 5) == 0 && memcmp(first.pan, second.pan, 5) == 0;
}

static uint16_t advance(uint16_t state) {
    const uint16_t feedback = (state ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1;
    return static_cast<uint16_t>((state >> 1) | (feedback << 15));
}

uint8_t beaconChannel(uint32_t seed, uint16_t sequence) {
    const uint16_t group = sequence / ChannelCount;
    uint16_t state = static_cast<uint16_t>((seed >> 16) ^ (seed & 0xFFFF) ^ group);
    state = advance(state);
    if (state == 0) state = 0xACE1;
    uint8_t channels[ChannelCount];
    for (uint8_t index = 0; index < ChannelCount; ++index) channels[index] = index;
    for (uint8_t index = 0; index < ChannelCount; ++index) {
        const uint8_t other = state % ChannelCount;
        const uint8_t previous = channels[index];
        channels[index] = channels[other];
        channels[other] = previous;
        state = advance(state);
    }
    return channels[sequence % ChannelCount];
}

uint32_t channelFrequency(uint8_t channel) {
    return channel < ChannelCount ? 902200000UL + static_cast<uint32_t>(channel) * 200000UL : 0;
}

static uint8_t hexDigit(char digit) {
    return static_cast<uint8_t>(digit <= '9' ? digit - '0' : digit - 'a' + 10);
}

static size_t readHex(const char* hex, uint8_t* bytes) {
    const size_t count = strlen(hex) / 2;
    for (size_t index = 0; index < count; ++index) {
        bytes[index] = static_cast<uint8_t>((hexDigit(hex[index * 2]) << 4) | hexDigit(hex[index * 2 + 1]));
    }
    return count;
}

static bool rejectsDamage(const uint8_t* original, size_t count) {
    Frame frame;
    for (size_t length = 0; length < count; ++length) {
        if (decodeAir(original, length, frame) == DecodeStatus::Ok || frame.length != 0) return false;
    }
    uint8_t changed[MaxAirBytes];
    memcpy(changed, original, count);
    // Exercise every transmitted PSDU bit, including each FCS bit.
    for (size_t index = 2; index < count; ++index) {
        for (uint8_t bit = 0; bit < 8; ++bit) {
            changed[index] ^= static_cast<uint8_t>(1 << bit);
            if (decodeAir(changed, count, frame) == DecodeStatus::Ok || frame.length != 0) return false;
            changed[index] = original[index];
        }
    }
    return true;
}

static bool messageSelfTest() {
    // Synthetic link-layer examples built from the published field diagrams.
    // They exercise header interpretation and bounds, and are not RF captures.
    Frame frame;
    Message message;
    frame.length = static_cast<uint16_t>(readHex("244ca0010203042081236adeadbeef11223344", frame.bytes));
    if (parseMessage(frame, message) != MessageStatus::Ok || !message.linkValid || !message.networkValid ||
        message.frameType != 1 || !message.ackRequested || message.source.format != 1 ||
        message.source.length != 5 || message.destination.format != 0 || message.destination.length != 1 ||
        message.destination.bytes[0] != 0x20 || message.sequence != 0x123 || message.sequenceBytes != 2 ||
        message.linkHeaderBytes != 10 || message.authenticationOffset != 11 || message.authenticationBytes != 4 ||
        !message.encrypted || message.ciphertextOffset != 15 || message.ciphertextLength != 4 ||
        message.payloadOffset != 15 || message.payloadLength != 4 || message.counterPresent) return false;

    // Header truncations fail. Once at least one payload byte is present, a
    // shorter payload is another structurally valid message, so it is accepted.
    for (uint16_t length = 0; length <= 15; ++length) {
        Frame truncated = frame;
        truncated.length = length;
        if (parseMessage(truncated, message) == MessageStatus::Ok || message.networkValid ||
            message.ciphertextLength != 0) return false;
    }
    frame.length = static_cast<uint16_t>(readHex("444ca0010203042081236b8001020304deadbeef11223344", frame.bytes));
    const uint8_t counter[] = {1, 2, 3, 4};
    if (parseMessage(frame, message) != MessageStatus::Ok || message.frameType != 2 ||
        !message.counterPresent || message.counterOffset != 12 || memcmp(message.counter, counter, 4) != 0 ||
        message.authenticationOffset != 16 || message.authenticationBytes != 4 ||
        message.ciphertextOffset != 20 || message.ciphertextLength != 4) return false;

    Frame changed = frame;
    changed.bytes[10] |= 0x80; // Fragmentation is reserved in this specification.
    if (parseMessage(changed, message) != MessageStatus::UnsupportedNetwork || !message.linkValid ||
        message.networkValid || message.ciphertextLength) return false;
    changed = frame;
    changed.bytes[11] = 0x81; // Security reference has reserved bits set.
    if (parseMessage(changed, message) != MessageStatus::UnsupportedNetwork || !message.linkValid) return false;
    changed = frame;
    changed.bytes[10] = 0x77; // A counter reference cannot have a 16-byte tag.
    if (parseMessage(changed, message) != MessageStatus::UnsupportedNetwork) return false;
    changed = frame;
    changed.bytes[10] = 0x7A; // Reserved authentication code size 6.
    if (parseMessage(changed, message) != MessageStatus::UnsupportedNetwork) return false;
    changed = frame;
    changed.bytes[10] = 0x63; // Extended header without any authentication code.
    if (parseMessage(changed, message) != MessageStatus::UnsupportedNetwork) return false;
    changed = frame;
    changed.bytes[1] |= 3;
    if (parseMessage(changed, message) != MessageStatus::InvalidHeader || message.linkValid) return false;
    changed = frame;
    changed.bytes[2] = 0xC0; // Six-byte address exceeds the published maximum.
    if (parseMessage(changed, message) != MessageStatus::InvalidHeader) return false;
    changed = frame;
    changed.bytes[8] = 0x01; // Zero encoded sequence length.
    if (parseMessage(changed, message) != MessageStatus::InvalidHeader) return false;

    frame.length = static_cast<uint16_t>(readHex("2408a00102030420404200", frame.bytes));
    if (parseMessage(frame, message) != MessageStatus::Ok || !message.networkValid || message.encrypted ||
        message.ciphertextLength || message.payloadOffset != 10 || message.payloadLength != 1 ||
        message.authenticationBytes != 0) return false;
    frame.length = static_cast<uint16_t>(readHex("6450a001020304a005060708c10203", frame.bytes));
    if (parseMessage(frame, message) != MessageStatus::Ok || message.frameType != 3 || !message.linkValid ||
        message.networkPresent || message.networkValid || message.sequence != 0x10203 ||
        message.source.length != 5 || message.destination.length != 5 || message.networkLength != 0) return false;
    changed = frame;
    changed.bytes[changed.length++] = 0;
    if (parseMessage(changed, message) != MessageStatus::InvalidHeader) return false;

    // Synthetic unsupported-version example retains raw bytes only.
    frame.length = static_cast<uint16_t>(readHex("4234010203040506070809", frame.bytes));
    if (parseMessage(frame, message) != MessageStatus::UnsupportedVersion || message.protocolVersion != 2 ||
        message.linkValid || message.networkValid || message.ciphertextLength) return false;
    frame.length = static_cast<uint16_t>(readHex("0400", frame.bytes));
    return parseMessage(frame, message) == MessageStatus::NotMessage;
}

bool selfTest() {
    // Silicon Labs AN633 Rev 0.9 p65. These bytes are published independently
    // of this decoder and cover both FCS sizes with and without whitening.
    static const char* const vectors[] = {
        "1005400056279e", "18054f70e548dd",
        "00074000565d29fa28", "08074f70e5326a6260"
    };
    const uint8_t expected[] = {0x40, 0x00, 0x56};
    uint8_t air[MaxAirBytes];
    Frame frame;
    for (const char* vector : vectors) {
        const size_t count = readHex(vector, air);
        if (decodeAir(air, count, frame) != DecodeStatus::Ok || frame.length != sizeof(expected) ||
            memcmp(frame.bytes, expected, sizeof(expected)) != 0 || !rejectsDamage(air, count)) return false;
    }

    // Synthetic beacon fixtures with fictional identifiers and tags.
    // PHY bytes and channel expectations were generated independently.
    static const char* const captures[] = {
        "18200b70136e419b4c0eadb50b593bb4b2612b103766643aebff3cf48af85ba2fc71",
        "18200b70136e419b4c0eadb50b593bb4b2612b103761643aebff3cf48af85ba2d5cc",
        "18200b70136a459f400ee9f14f953bb427303e493720643aebff3cf48af85ba29abd",
        "18200b70136e419b4c0eadb50b593bb4b2612b103762643aebff3cf48af85ba2ad36"
    };
    static const char* const linkBytes[] = {
        "0400a001020304a011223344e86012345678000b04810832093201020304",
        "0400a001020304a011223344e86012345678000c04810832093201020304",
        "0400a005060708a055667788e86087654321004d04810832093201020304",
        "0400a001020304a011223344e86012345678000f04810832093201020304"
    };
    static const uint16_t sequences[] = {11, 12, 77, 15};
    static const uint8_t channels[] = {12, 30, 35, 6};
    Beacon first;
    for (size_t index = 0; index < 4; ++index) {
        const size_t count = readHex(captures[index], air);
        if (decodeAir(air, count, frame) != DecodeStatus::Ok || !rejectsDamage(air, count)) return false;
        uint8_t expectedLink[MaxPsdu];
        const size_t expectedLength = readHex(linkBytes[index], expectedLink);
        if (frame.length != expectedLength || memcmp(frame.bytes, expectedLink, expectedLength) != 0) return false;
        Beacon beacon;
        if (!parseBeacon(frame, beacon) || beacon.sequence != sequences[index] ||
            beaconChannel(beacon.seed, beacon.sequence) != channels[index] || !beacon.hasSchedule ||
            beacon.csUlOffset != 8 || beacon.csUlPeriodicity != 50 || beacon.csDlOffset != 9 ||
            beacon.csDlPeriodicity != 50 || beacon.authenticationTagBytes != 4) return false;
        const uint32_t expectedSeed = index == 2 ? 0x87654321UL : 0x12345678UL;
        if (beacon.seed != expectedSeed) return false;
        if (index == 0) first = beacon;
        if (sameGateway(first, beacon) != (index != 2)) return false;
    }

    // Group boundaries and uint16 sequence rollover, calculated independently
    // with the handoff's standalone Python reference implementation.
    static const uint16_t boundarySequences[] = {0, 68, 69, 137, 138, 65535};
    static const uint8_t boundaryChannels[] = {48, 10, 41, 1, 3, 7};
    for (size_t index = 0; index < 6; ++index) {
        if (beaconChannel(0x12345678UL, boundarySequences[index]) != boundaryChannels[index]) return false;
    }
    if (beaconChannel(0, 0) != 28 || channelFrequency(0) != 902200000UL ||
        channelFrequency(68) != 915800000UL || channelFrequency(69) != 0) return false;

    // A structurally invalid beacon must not enter the scheduling cache even
    // if a sender supplied a valid PHY FCS around the malformed link bytes.
    readHex(linkBytes[0], frame.bytes);
    frame.length = 30;
    Beacon beacon;
    for (size_t length = 0; length < 30; ++length) {
        Frame truncated = frame;
        truncated.length = static_cast<uint16_t>(length);
        if (parseBeacon(truncated, beacon)) return false;
    }
    Frame changed = frame;
    changed.bytes[0] = 0x24;
    if (parseBeacon(changed, beacon)) return false;
    changed = frame;
    changed.bytes[1] = 1;
    if (parseBeacon(changed, beacon)) return false;
    changed = frame;
    changed.bytes[12] |= 1;
    if (parseBeacon(changed, beacon)) return false;
    changed = frame;
    changed.bytes[20] |= 0x80;
    if (parseBeacon(changed, beacon)) return false;
    changed = frame;
    changed.bytes[20] &= static_cast<uint8_t>(~4);
    if (parseBeacon(changed, beacon)) return false;
    changed = frame;
    changed.bytes[21] = 0xC1;
    changed.bytes[22] = 255;
    if (parseBeacon(changed, beacon)) return false;
    const uint8_t invalidHeaders[][2] = {{0x80, 5}, {0x20, 5}, {0x10, 2}, {0, 4}, {0x10, 252}};
    uint16_t total;
    for (const auto& header : invalidHeaders) {
        if (inspectHeader(header, 2, total) == DecodeStatus::Ok || total != 0) return false;
    }
    return messageSelfTest();
}

}  // namespace sidewalk
