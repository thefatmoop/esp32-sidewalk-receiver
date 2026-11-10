#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <TinyGPSPlus.h>
#include <esp_timer.h>
#include <sys/time.h>
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#include "Secrets.example.h"
#endif
#include "Sidewalk.h"
#include "DataSchedule.h"
#include "Sx1276Fsk.h"
#include "WebUi.h"

namespace {
constexpr size_t GatewayCount = 12;
constexpr size_t PacketCount = 48;
constexpr size_t DataPacketCount = 32;
constexpr uint32_t BeaconPeriodMs = sidewalk::BeaconIntervalUs / 1000;
constexpr uint32_t ScanDwellMs = 25;
constexpr uint32_t BeaconLeadMs = 220;
constexpr uint32_t BeaconLateMs = 100;
constexpr uint8_t MaximumMisses = 6;

enum class Mode { Scan, Hold, Follow };
struct Gateway {
    bool used = false;
    sidewalk::Beacon beacon;
    char id[11] = {};
    char pan[11] = {};
    uint32_t lastSeenMs = 0;
    uint32_t lastSfdUs = 0;
    uint64_t lastSeenEpochMs = 0;
    uint32_t lastFrequencyHz = 0;
    int16_t rssiDbm = 0;
    uint32_t packets = 0;
    uint32_t matches = 0;
    uint32_t misses = 0;
    uint8_t consecutiveMisses = 0;
    uint16_t nextSequence = 0;
    uint32_t nextDueMs = 0;
    bool scheduled = false;
    uint32_t dataFrames = 0;
    bool dataPlanned = false;
    uint32_t nextDataCheckUs = 0;
    sidewalk::DataOpportunity nextData;
};
struct Packet {
    uint32_t id = 0;
    uint32_t uptimeMs = 0;
    uint64_t epochMs = 0;
    uint32_t frequencyHz = 0;
    int16_t rssiDbm = 0;
    uint16_t length = 0;
    bool beacon = false;
    sidewalk::Message message;
    sidewalk::MessageStatus parseStatus = sidewalk::MessageStatus::NotMessage;
    uint8_t association = 0;
    const char* direction = "unknown";
    char gateway[11] = {};
    char pan[11] = {};
    uint32_t sequence = 0;
    char hex[sidewalk::MaxPsdu * 2 + 1] = {};
};
struct DataListen {
    bool active = false;
    char gateway[11] = {};
    char pan[11] = {};
    sidewalk::DataOpportunity opportunity;
};
struct Metrics {
    bool radioOk = false;
    uint8_t radioVersion = 0;
    Mode mode = Mode::Scan;
    uint32_t frequencyHz = 903200000;
    int16_t rssiDbm = -127;
    uint32_t crcOk = 0;
    uint32_t crcFail = 0;
    uint32_t syncs = 0;
    uint32_t overflows = 0;
    uint32_t invalidHeaders = 0;
    uint32_t timeouts = 0;
    uint32_t misses = 0;
};
struct State : Metrics {
    Gateway gateways[GatewayCount];
    Packet packets[PacketCount];
    Packet dataPackets[DataPacketCount];
    DataListen dataListen;
    uint32_t dataPacketCount = 0;
    uint32_t packetId = 0;
};
struct Command { Mode mode; uint32_t frequencyHz; };
State state;
SemaphoreHandle_t stateMutex;
QueueHandle_t commandQueue;
WebServer server(80);
TinyGPSPlus gps;
Sx1276Fsk radio;
bool protocolOk = false;
bool mdnsStarted = false;
uint32_t lastWifiAttempt = 0;

uint64_t epochNowMs() {
    timeval value;
    gettimeofday(&value, nullptr);
    if (value.tv_sec < 1700000000) return 0;
    return uint64_t(value.tv_sec) * 1000 + value.tv_usec / 1000;
}

void hexBytes(const uint8_t* bytes, size_t length, char* output) {
    constexpr char digits[] = "0123456789ABCDEF";
    for (size_t i = 0; i < length; ++i) {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 15];
    }
    output[length * 2] = 0;
}

// Every value appended here comes from radio metadata or local configuration.
String jsonString(const String& value) {
    String result = "\"";
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '"' || c == '\\') result += '\\';
        if (uint8_t(c) >= 32) result += c;
    }
    return result + '"';
}

String number64(uint64_t value) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    return String(buffer);
}

uint32_t predictedFrequency(const Gateway& gateway) {
    return sidewalk::channelFrequency(sidewalk::beaconChannel(gateway.beacon.seed, gateway.nextSequence));
}

bool addressMatches(const sidewalk::Address& address, const Gateway& gateway) {
    return address.format == 1 && address.length == 5 && memcmp(address.bytes, gateway.beacon.source, 5) == 0;
}

void processPacket(const Sx1276Fsk::Packet& received, const DataListen& listening) {
    sidewalk::Frame frame;
    const auto result = sidewalk::decodeAir(received.bytes, received.length, frame);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (result != sidewalk::DecodeStatus::Ok) {
        if (result == sidewalk::DecodeStatus::CrcMismatch) ++state.crcFail;
        xSemaphoreGive(stateMutex);
        return;
    }
    ++state.crcOk;
    // syncUs wraps every 71 minutes. Unsigned subtraction extends it locally.
    const uint32_t elapsedUs = uint32_t(micros() - received.syncUs);
    const uint32_t sourceMs = millis() - elapsedUs / 1000;
    const uint64_t currentEpoch = epochNowMs();
    const uint64_t sourceEpoch = currentEpoch ? currentEpoch - elapsedUs / 1000 : 0;
    sidewalk::Beacon beacon;
    const bool isBeacon = sidewalk::parseBeacon(frame, beacon);
    Packet& packet = state.packets[state.packetId % PacketCount];
    packet = Packet{};
    packet.id = ++state.packetId;
    packet.uptimeMs = sourceMs;
    packet.epochMs = sourceEpoch;
    packet.frequencyHz = received.frequencyHz;
    packet.rssiDbm = received.rssiDbm;
    packet.length = frame.length;
    packet.beacon = isBeacon;
    hexBytes(frame.bytes, frame.length, packet.hex);
    if (isBeacon) {
        hexBytes(beacon.source, 5, packet.gateway);
        hexBytes(beacon.pan, 5, packet.pan);
        packet.sequence = beacon.sequence;
        Gateway* gateway = nullptr;
        Gateway* replacement = &state.gateways[0];
        for (auto& candidate : state.gateways) {
            if (candidate.used && sidewalk::sameGateway(candidate.beacon, beacon)) gateway = &candidate;
            if (!candidate.used || (replacement->used && int32_t(candidate.lastSeenMs - replacement->lastSeenMs) < 0)) replacement = &candidate;
        }
        if (!gateway) {
            gateway = replacement;
            *gateway = Gateway{};
            gateway->used = true;
        }
        const uint16_t sequenceDelta = uint16_t(beacon.sequence - gateway->beacon.sequence);
        const bool fresh = gateway->packets == 0 || int32_t(sourceMs - gateway->lastSeenMs) > 0;
        const bool inOrder = gateway->packets == 0 || (sequenceDelta > 0 && sequenceDelta < 0x8000);
        const bool rebaseline = !gateway->scheduled || sourceMs - gateway->lastSeenMs > 30000 || beacon.seed != gateway->beacon.seed;
        if (fresh && (inOrder || rebaseline)) {
            const uint32_t expectedHz = sidewalk::channelFrequency(sidewalk::beaconChannel(beacon.seed, beacon.sequence));
            const bool channelMatches = abs(int32_t(expectedHz - received.frequencyHz)) < 3000;
            if (rebaseline) gateway->matches = 0;
            gateway->beacon = beacon;
            hexBytes(beacon.source, 5, gateway->id);
            hexBytes(beacon.pan, 5, gateway->pan);
            gateway->lastSeenMs = sourceMs;
            gateway->lastSfdUs = received.syncUs;
            gateway->lastSeenEpochMs = sourceEpoch;
            gateway->lastFrequencyHz = received.frequencyHz;
            gateway->rssiDbm = received.rssiDbm;
            ++gateway->packets;
            if (channelMatches) ++gateway->matches;
            else gateway->matches = 0;
            gateway->consecutiveMisses = 0;
            gateway->nextSequence = uint16_t(beacon.sequence + 1);
            gateway->nextDueMs = sourceMs + BeaconPeriodMs;
            // Published vectors and captured beacons are checked at startup.
            // Each live beacon must also predict its actually received channel.
            gateway->scheduled = protocolOk && channelMatches;
            gateway->dataPlanned = false;
            gateway->nextDataCheckUs = 0;
        }
    } else {
        packet.parseStatus = sidewalk::parseMessage(frame, packet.message);
        int explicitGateway = -1;
        uint8_t explicitMatches = 0;
        for (size_t index = 0; index < GatewayCount; ++index) {
            const auto& candidate = state.gateways[index];
            if (!candidate.used || !packet.message.linkValid) continue;
            if (addressMatches(packet.message.source, candidate) || addressMatches(packet.message.destination, candidate)) {
                explicitGateway = index;
                ++explicitMatches;
            }
        }
        if (explicitMatches == 1) {
            const auto& gateway = state.gateways[explicitGateway];
            memcpy(packet.gateway, gateway.id, sizeof(packet.gateway));
            memcpy(packet.pan, gateway.pan, sizeof(packet.pan));
            packet.association = 1;
            packet.direction = addressMatches(packet.message.source, gateway) ? "downlink" : "uplink";
        } else if (explicitMatches == 0 && listening.active && listening.opportunity.frequencyHz == received.frequencyHz &&
                   int32_t(received.syncUs - listening.opportunity.openUs) >= 0 &&
                   int32_t(listening.opportunity.closeUs - received.syncUs) >= 0) {
            memcpy(packet.gateway, listening.gateway, sizeof(packet.gateway));
            memcpy(packet.pan, listening.pan, sizeof(packet.pan));
            packet.association = 2;
        }
        if (packet.message.linkValid) {
            packet.sequence = packet.message.sequence;
            if (packet.message.source.format == 0 && packet.message.destination.format == 1) packet.direction = "downlink";
            if (packet.message.source.format == 1 && packet.message.destination.format == 0) packet.direction = "uplink";
        }
        for (auto& gateway : state.gateways) {
            if (gateway.used && strcmp(gateway.id, packet.gateway) == 0 && strcmp(gateway.pan, packet.pan) == 0) ++gateway.dataFrames;
        }
        state.dataPackets[state.dataPacketCount % DataPacketCount] = packet;
        ++state.dataPacketCount;
    }
    // Serial publication runs outside this task so USB cannot stall FIFO reads.
    xSemaphoreGive(stateMutex);
    digitalWrite(2, HIGH);
}

void radioTask(void*) {
    protocolOk = sidewalk::selfTest() && sidewalk::dataScheduleSelfTest();
    const bool initialized = protocolOk && radio.begin(903200000);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    state.radioOk = initialized;
    state.radioVersion = radio.version();
    xSemaphoreGive(stateMutex);
    Mode mode = Mode::Scan;
    uint8_t scanChannel = 5;
    uint32_t dwellStart = millis();
    uint32_t lastDiagnostics = 0;
    uint32_t lastGenericMs = 0;
    size_t genericCursor = 0;
    DataListen listening;
    for (;;) {
        Command command;
        if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
            mode = command.mode;
            listening = DataListen{};
            if (initialized && command.frequencyHz) radio.tune(command.frequencyHz);
            dwellStart = millis();
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            state.mode = mode;
            xSemaphoreGive(stateMutex);
        }
        if (!initialized) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        Sx1276Fsk::Packet received;
        if (radio.poll(received)) processPacket(received, listening);
        const uint32_t now = millis();
        uint32_t targetHz = 0;
        bool opportunity = false;
        int32_t earliest = INT32_MAX;
        const uint32_t nowUs = micros();
        DataListen commonListening;
        int32_t earliestDataClose = INT32_MAX;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        for (auto& gateway : state.gateways) {
            if (!gateway.used || !gateway.scheduled) continue;
            if (int32_t(now - gateway.nextDueMs) > int32_t(BeaconLateMs)) {
                const uint32_t elapsedPeriods = uint32_t(now - gateway.nextDueMs - BeaconLateMs - 1) / BeaconPeriodMs + 1;
                if (mode != Mode::Hold) {
                    gateway.misses += elapsedPeriods;
                    state.misses += elapsedPeriods;
                    gateway.consecutiveMisses = uint8_t(min(uint32_t(MaximumMisses), uint32_t(gateway.consecutiveMisses) + elapsedPeriods));
                }
                gateway.nextSequence = uint16_t(gateway.nextSequence + elapsedPeriods);
                gateway.nextDueMs += elapsedPeriods * BeaconPeriodMs;
                if (gateway.consecutiveMisses >= MaximumMisses) gateway.scheduled = false;
            }
            const int32_t until = int32_t(gateway.nextDueMs - now);
            const int32_t lead = mode == Mode::Follow ? 350 : BeaconLeadMs;
            if (gateway.scheduled && until < lead && until < earliest) {
                earliest = until;
                targetHz = predictedFrequency(gateway);
                opportunity = true;
            }
            if (gateway.scheduled && gateway.beacon.hasSchedule && !radio.busy()) {
                if ((gateway.dataPlanned && int32_t(nowUs - gateway.nextData.closeUs) > 0) ||
                    (!gateway.dataPlanned && (gateway.nextDataCheckUs == 0 || int32_t(nowUs - gateway.nextDataCheckUs) >= 0))) {
                    gateway.dataPlanned = sidewalk::nextCommonOpportunity(gateway.beacon, gateway.lastSfdUs, nowUs, gateway.nextData);
                    gateway.nextDataCheckUs = nowUs + 1000000;
                }
                if (gateway.dataPlanned && int32_t(nowUs - gateway.nextData.openUs) >= 0 &&
                    int32_t(gateway.nextData.closeUs - nowUs) >= 0 &&
                    int32_t(gateway.nextData.closeUs - nowUs) < earliestDataClose) {
                    earliestDataClose = int32_t(gateway.nextData.closeUs - nowUs);
                    commonListening.active = true;
                    memcpy(commonListening.gateway, gateway.id, sizeof(commonListening.gateway));
                    memcpy(commonListening.pan, gateway.pan, sizeof(commonListening.pan));
                    commonListening.opportunity = gateway.nextData;
                }
            }
        }
        state.frequencyHz = radio.frequencyHz();
        if (now - lastDiagnostics >= 100) {
            state.rssiDbm = radio.rssiDbm();
            state.syncs = radio.counters.syncs;
            state.overflows = radio.counters.overflows;
            state.invalidHeaders = radio.counters.invalidHeaders;
            state.timeouts = radio.counters.timeouts;
            lastDiagnostics = now;
        }
        xSemaphoreGive(stateMutex);
        if (mode != Mode::Hold) {
            if (opportunity) {
                if (!radio.busy()) {
                    if (targetHz != radio.frequencyHz()) radio.tune(targetHz);
                    listening = DataListen{};
                }
                dwellStart = now;
            } else if (!radio.busy()) {
                if (commonListening.active) listening = commonListening;
                else if (listening.active && int32_t(nowUs - listening.opportunity.closeUs) > 0) listening = DataListen{};
                // Sample additional hopping slots for privately negotiated schedules.
                // Rotate gateways and leave RF time available for discovery scanning.
                if (!listening.active && now - lastGenericMs >= 250) {
                    for (size_t offset = 0; offset < GatewayCount; ++offset) {
                        const size_t index = (genericCursor + offset) % GatewayCount;
                        const Gateway& gateway = state.gateways[index];
                        if (!gateway.used || !gateway.scheduled) continue;
                        const int32_t sinceZero = int32_t(nowUs - gateway.lastSfdUs) - sidewalk::DefaultSlotZeroAfterSfdUs;
                        if (sinceZero < 0) continue;
                        const uint32_t absoluteSlot = uint32_t(sinceZero) / sidewalk::SlotIntervalUs;
                        const uint32_t inSlot = uint32_t(sinceZero) % sidewalk::SlotIntervalUs;
                        const uint32_t chosen = absoluteSlot + (inSlot > 4000 ? 1 : 0);
                        sidewalk::DataOpportunity candidate;
                        const uint16_t sequence = uint16_t(gateway.beacon.sequence + chosen / sidewalk::SlotsPerBeacon);
                        const uint8_t slot = chosen % sidewalk::SlotsPerBeacon;
                        if (!sidewalk::opportunityForSlot(gateway.beacon, gateway.lastSfdUs, sequence, slot, candidate)) continue;
                        if (int32_t(nowUs - candidate.openUs) < 0 || int32_t(candidate.closeUs - nowUs) < 0) continue;
                        listening.active = true;
                        memcpy(listening.gateway, gateway.id, sizeof(listening.gateway));
                        memcpy(listening.pan, gateway.pan, sizeof(listening.pan));
                        listening.opportunity = candidate;
                        genericCursor = (index + 1) % GatewayCount;
                        lastGenericMs = now;
                        break;
                    }
                }
                if (listening.active) {
                    if (radio.frequencyHz() != listening.opportunity.frequencyHz) radio.tune(listening.opportunity.frequencyHz);
                    dwellStart = now;
                } else if (now - dwellStart >= ScanDwellMs) {
                    scanChannel = uint8_t((scanChannel + 1) % sidewalk::ChannelCount);
                    radio.tune(sidewalk::channelFrequency(scanChannel));
                    dwellStart = now;
                }
            }
        }
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        state.dataListen = listening;
        state.frequencyHz = radio.frequencyHz();
        xSemaphoreGive(stateMutex);
        vTaskDelay(1);
    }
}

const char* modeName(Mode mode) {
    if (mode == Mode::Hold) return "hold";
    if (mode == Mode::Follow) return "follow";
    return "scan";
}

void statusRequest() {
    // Copy only status and gateway records, never the packet ring onto the stack.
    static Gateway gateways[GatewayCount];
    DataListen dataListen;
    uint32_t dataPacketCount;
    Metrics metrics;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    memcpy(gateways, state.gateways, sizeof(gateways));
    metrics = state;
    dataListen = state.dataListen;
    dataPacketCount = state.dataPacketCount;
    xSemaphoreGive(stateMutex);
    String json;
    json.reserve(9000);
    json = "{\"uptimeMs\":" + String(millis()) + ",\"epochMs\":" + number64(epochNowMs());
    json += ",\"radioOk\":" + String(metrics.radioOk ? "true" : "false");
    json += ",\"radioVersion\":" + String(metrics.radioVersion);
    json += ",\"mode\":" + jsonString(modeName(metrics.mode));
    json += ",\"frequencyHz\":" + String(metrics.frequencyHz) + ",\"rssiDbm\":" + String(metrics.rssiDbm);
    json += ",\"crcOk\":" + String(metrics.crcOk) + ",\"crcFail\":" + String(metrics.crcFail);
    json += ",\"syncs\":" + String(metrics.syncs) + ",\"overflows\":" + String(metrics.overflows);
    json += ",\"invalidHeaders\":" + String(metrics.invalidHeaders) + ",\"timeouts\":" + String(metrics.timeouts);
    json += ",\"misses\":" + String(metrics.misses);
    const uint32_t now = millis();
    const Gateway* nearest = nullptr;
    int32_t nearestDelta = INT32_MAX;
    json += ",\"gateways\":[";
    bool first = true;
    for (const auto& gateway : gateways) {
        if (!gateway.used) continue;
        if (!first) json += ',';
        first = false;
        const int32_t remaining = int32_t(gateway.nextDueMs - now);
        if (gateway.scheduled && remaining < nearestDelta) { nearest = &gateway; nearestDelta = remaining; }
        json += "{\"id\":" + jsonString(gateway.id) + ",\"pan\":" + jsonString(gateway.pan);
        json += ",\"sequence\":" + String(gateway.beacon.sequence) + ",\"seed\":" + String(gateway.beacon.seed);
        json += ",\"matches\":" + String(gateway.matches) + ",\"packets\":" + String(gateway.packets);
        json += ",\"lastSeenMs\":" + String(gateway.lastSeenMs) + ",\"lastSeenEpochMs\":" + number64(gateway.lastSeenEpochMs);
        json += ",\"lastFrequencyHz\":" + String(gateway.lastFrequencyHz) + ",\"rssiDbm\":" + String(gateway.rssiDbm);
        json += ",\"nextFrequencyHz\":" + String(gateway.scheduled ? predictedFrequency(gateway) : 0);
        json += ",\"nextInMs\":" + String(gateway.scheduled ? remaining : 0);
        json += ",\"misses\":" + String(gateway.misses) + ",\"locked\":" + String(gateway.scheduled && gateway.matches >= 3 ? "true" : "false");
        json += ",\"cloudConnection\":" + String(gateway.beacon.cloudConnection);
        json += ",\"roamingAllowed\":" + String(gateway.beacon.roamingAllowed ? "true" : "false");
        json += ",\"loadIndicator\":" + String(gateway.beacon.loadIndicator);
        json += ",\"advertisedPowerDbm\":" + String(gateway.beacon.downlinkEirpDbm);
        json += ",\"uplinkAdjustmentDb\":" + String(gateway.beacon.uplinkAdjustmentDb);
        json += ",\"authenticationTagBytes\":" + String(gateway.beacon.authenticationTagBytes);
        json += ",\"hasSchedule\":" + String(gateway.beacon.hasSchedule ? "true" : "false");
        json += ",\"csUlOffset\":" + String(gateway.beacon.csUlOffset) + ",\"csUlPeriodicity\":" + String(gateway.beacon.csUlPeriodicity);
        json += ",\"csDlOffset\":" + String(gateway.beacon.csDlOffset) + ",\"csDlPeriodicity\":" + String(gateway.beacon.csDlPeriodicity);
        json += ",\"dataFrames\":" + String(gateway.dataFrames) + '}';
    }
    json += "],\"next\":";
    if (nearest) {
        json += "{\"frequencyHz\":" + String(predictedFrequency(*nearest)) + ",\"inMs\":" + String(nearestDelta);
        json += ",\"sequence\":" + String(nearest->nextSequence) + ",\"gateway\":" + jsonString(nearest->id) + '}';
    } else json += "null";
    json += ",\"dataFrames\":" + String(dataPacketCount) + ",\"dataListen\":";
    if (dataListen.active) {
        const auto& target = dataListen.opportunity;
        const char* direction = "shared";
        if (target.directions == 1) direction = "uplink";
        if (target.directions == 2) direction = "downlink";
        json += "{\"gateway\":" + jsonString(dataListen.gateway) + ",\"pan\":" + jsonString(dataListen.pan);
        json += ",\"frequencyHz\":" + String(target.frequencyHz) + ",\"slot\":" + String(target.slot);
        json += ",\"sequence\":" + String(target.beaconSequence) + ",\"direction\":" + jsonString(direction);
        json += ",\"inMs\":" + String(int32_t(target.targetUs - micros()) / 1000);
        json += ",\"timingEstimated\":true}";
    } else json += "null";
    json += ",\"gateway\":null,\"wifi\":{\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += ",\"ip\":" + jsonString(WiFi.localIP().toString()) + ",\"ssid\":" + jsonString(WIFI_SSID);
    json += ",\"rssi\":" + String(WiFi.RSSI()) + "},\"gps\":{\"valid\":";
    json += gps.location.isValid() && gps.location.age() < 5000 ? "true" : "false";
    json += ",\"lat\":" + String(gps.location.lat(), 6) + ",\"lon\":" + String(gps.location.lng(), 6);
    json += ",\"satellites\":" + String(gps.satellites.value()) + "},\"message\":";
    json += jsonString(metrics.radioOk ? "50 kb/s SubG-FSK · beacon + data slots · CRC checked · authentication tags unverified" : "Receiver unavailable. Check serial diagnostics and radio wiring.");
    json += ",\"freeHeap\":" + String(ESP.getFreeHeap()) + '}';
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

String packetJson(const Packet& packet) {
    String json;
    json.reserve(2200);
    const auto& message = packet.message;
    const char* kind = "unknown";
    if (packet.beacon) kind = "beacon";
    else if (message.linkValid) {
        if (message.frameType == 1) kind = "data";
        if (message.frameType == 2) kind = "network";
        if (message.frameType == 3) kind = "ack";
    }
    json = "{\"id\":" + String(packet.id) + ",\"uptimeMs\":" + String(packet.uptimeMs);
    json += ",\"epochMs\":" + number64(packet.epochMs) + ",\"frequencyHz\":" + String(packet.frequencyHz);
    json += ",\"rssiDbm\":" + String(packet.rssiDbm) + ",\"length\":" + String(packet.length);
    json += ",\"kind\":" + jsonString(kind) + ",\"gateway\":" + jsonString(packet.gateway);
    json += ",\"pan\":" + jsonString(packet.pan) + ",\"sequence\":" + String(packet.sequence);
    json += ",\"hex\":" + jsonString(packet.hex);
    const char* association = "none";
    if (packet.beacon || packet.association == 1) association = "explicit";
    if (packet.association == 2) association = "scheduled";
    json += ",\"association\":" + jsonString(association) + ",\"direction\":" + jsonString(packet.direction);
    if (!packet.beacon) {
        char source[11];
        char destination[11];
        hexBytes(message.source.bytes, message.source.length, source);
        hexBytes(message.destination.bytes, message.destination.length, destination);
        json += ",\"data\":{\"sourceId\":" + (message.linkValid ? jsonString(source) : String("null"));
        json += ",\"destinationId\":" + (message.linkValid ? jsonString(destination) : String("null"));
        json += ",\"linkValid\":" + String(message.linkValid ? "true" : "false");
        json += ",\"sourceFormat\":" + (message.linkValid ? String(message.source.format) : String("null"));
        json += ",\"destinationFormat\":" + (message.linkValid ? String(message.destination.format) : String("null"));
        json += ",\"sequence\":" + (message.linkValid ? String(message.sequence) : String("null"));
        json += ",\"frameType\":" + String(message.frameType) + ",\"frameTypeName\":";
        json += jsonString(message.linkValid ? sidewalk::frameTypeName(message.frameType) : "Unparsed frame");
        json += ",\"protocolVersion\":" + String(message.protocolVersion);
        json += ",\"parseStatus\":" + jsonString(sidewalk::messageStatusName(packet.parseStatus));
        json += ",\"networkValid\":" + String(message.networkValid ? "true" : "false");
        json += ",\"encrypted\":" + String(message.networkValid ? (message.encrypted ? "true" : "false") : "null");
        json += ",\"ackRequested\":" + String(message.linkValid ? (message.ackRequested ? "true" : "false") : "null");
        json += ",\"epoch\":" + String(message.linkValid ? (message.epoch ? "true" : "false") : "null");
        json += ",\"headerLength\":" + (message.linkValid ? String(message.linkHeaderBytes) : String("null"));
        json += ",\"securityTopology\":" + (message.networkValid ? String(message.securityTopology) : String("null"));
        json += ",\"extendedHeader\":" + String(message.networkValid ? (message.extendedHeader ? "true" : "false") : "null");
        json += ",\"payloadLength\":" + (message.networkValid ? String(message.payloadLength) : String("null")) + ",\"payloadHex\":";
        if (message.networkValid) json += jsonString(String(packet.hex).substring(message.payloadOffset * 2, (message.payloadOffset + message.payloadLength) * 2));
        else json += "null";
        json += ",\"authenticationTagHex\":";
        if (message.networkValid) json += jsonString(String(packet.hex).substring(message.authenticationOffset * 2, (message.authenticationOffset + message.authenticationBytes) * 2));
        else json += "null";
        json += ",\"counterHex\":";
        if (message.counterPresent && message.networkValid) {
            char counter[9];
            hexBytes(message.counter, 4, counter);
            json += jsonString(counter);
        } else json += "null";
        json += '}';
    }
    return json + '}';
}

void packetsRequest(bool dataOnly = false) {
    const uint32_t after = strtoul(server.arg("after").c_str(), nullptr, 10);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    const uint32_t newest = state.packetId;
    const uint32_t dataCount = state.dataPacketCount;
    const uint32_t retainedData = min(dataCount, uint32_t(DataPacketCount));
    const uint32_t dataStart = dataCount - retainedData;
    const uint32_t oldestDataId = retainedData ? state.dataPackets[dataStart % DataPacketCount].id : 0;
    xSemaphoreGive(stateMutex);
    const uint32_t oldest = newest > PacketCount ? newest - PacketCount + 1 : 1;
    // Chunk output so a full ring does not require a second large heap buffer.
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
    server.sendContent("{\"oldestId\":" + String(dataOnly ? oldestDataId : (newest ? oldest : 0)) + ",\"packets\":[");
    bool first = true;
    const uint32_t count = dataOnly ? retainedData : newest - oldest + 1;
    for (uint32_t index = 0; index < count; ++index) {
        Packet packet;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        if (dataOnly) packet = state.dataPackets[(dataStart + index) % DataPacketCount];
        else packet = state.packets[(oldest + index - 1) % PacketCount];
        xSemaphoreGive(stateMutex);
        if (!packet.id || packet.id <= after || packet.id > newest) continue;
        if (!first) server.sendContent(",");
        first = false;
        server.sendContent(packetJson(packet));
    }
    server.sendContent("]}");
    server.sendContent("");
}

void controlRequest() {
    Command command{Mode::Scan, 0};
    const String action = server.arg("action");
    if (action == "hold" || action == "tune") command.mode = Mode::Hold;
    else if (action == "follow") command.mode = Mode::Follow;
    else if (action != "scan") { server.send(400, "application/json", "{\"error\":\"Unknown action\"}"); return; }
    if (action == "tune") {
        char* end = nullptr;
        const String input = server.arg("mhz");
        const double mhz = strtod(input.c_str(), &end);
        if (end == input.c_str() || *end || !isfinite(mhz) || mhz < 902 || mhz > 928) {
            server.send(400, "application/json", "{\"error\":\"Frequency must be 902–928 MHz\"}"); return;
        }
        command.frequencyHz = uint32_t(mhz * 1000000.0 + 0.5);
    }
    if (xQueueSend(commandQueue, &command, 0) != pdTRUE) {
        server.send(503, "application/json", "{\"error\":\"Receiver command queue full\"}"); return;
    }
    server.send(200, "application/json", "{\"ok\":true}");
}
} // namespace

void setup() {
    Serial.begin(115200);
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);
    Serial2.setRxBufferSize(2048);
    Serial2.begin(9600, SERIAL_8N1, 34, 12);
    stateMutex = xSemaphoreCreateMutex();
    commandQueue = xQueueCreate(4, sizeof(Command));
    if (!stateMutex || !commandQueue) { Serial.println("Allocation failed"); return; }
    // High priority on the application core keeps the FIFO serviced during HTTP.
    if (xTaskCreatePinnedToCore(radioTask, "fsk-rx", 8192, nullptr, 3, nullptr, 1) != pdPASS) {
        Serial.println("Radio task allocation failed");
    }
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("sidewalk-rx");
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttempt = millis();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    server.on("/", HTTP_GET, [] { server.sendHeader("Cache-Control", "no-cache"); server.send_P(200, "text/html; charset=utf-8", WEB_UI); });
    server.on("/api/status", HTTP_GET, statusRequest);
    server.on("/api/packets", HTTP_GET, [] { packetsRequest(); });
    server.on("/api/data", HTTP_GET, [] { packetsRequest(true); });
    server.on("/api/control", HTTP_POST, controlRequest);
    server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
    server.begin();
    Serial.println("Sidewalk FSK monitor starting. GPIO5/19/25/18/16/27. RX only.");
}

void loop() {
    while (Serial2.available()) gps.encode(Serial2.read());
    server.handleClient();
    static uint32_t printedId = 0;
    static uint32_t lastStatus = 0;
    const uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
        mdnsStarted = MDNS.begin("sidewalk-rx");
        if (mdnsStarted) MDNS.addService("http", "tcp", 80);
        Serial.print("Web interface http://"); Serial.println(WiFi.localIP());
    }
    if (WiFi.status() != WL_CONNECTED && now - lastWifiAttempt > 30000) {
        WiFi.reconnect();
        lastWifiAttempt = now;
    }
    if (now - lastStatus >= 10000) {
        Metrics metrics;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        metrics = state;
        xSemaphoreGive(stateMutex);
        Serial.printf("RX radio=%s version=%02X selftest=%s freq=%lu sync=%lu crc_ok=%lu crc_bad=%lu headers=%lu overrun=%lu heap=%u\n",
            metrics.radioOk ? "ok" : "failed", metrics.radioVersion, protocolOk ? "pass" : "FAIL",
            static_cast<unsigned long>(metrics.frequencyHz), static_cast<unsigned long>(metrics.syncs),
            static_cast<unsigned long>(metrics.crcOk), static_cast<unsigned long>(metrics.crcFail),
            static_cast<unsigned long>(metrics.invalidHeaders), static_cast<unsigned long>(metrics.overflows), ESP.getFreeHeap());
        lastStatus = now;
    }
    Packet packet;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    const uint32_t newest = state.packetId;
    if (printedId < newest) {
        printedId = max(printedId + 1, newest > PacketCount ? newest - PacketCount + 1 : 1U);
        packet = state.packets[(printedId - 1) % PacketCount];
    }
    xSemaphoreGive(stateMutex);
    if (packet.id) {
        Serial.printf("PACKET id=%lu t=%lu freq=%lu rssi=%d type=%s gateway=%s seq=%u bytes=%s\n",
            static_cast<unsigned long>(packet.id), static_cast<unsigned long>(packet.uptimeMs),
            static_cast<unsigned long>(packet.frequencyHz), packet.rssiDbm,
            packet.beacon ? "beacon" : (packet.message.linkValid ? sidewalk::frameTypeName(packet.message.frameType) : "unclassified"),
            packet.gateway, packet.sequence, packet.hex);
        if (!packet.beacon && packet.message.networkValid) {
            const auto& message = packet.message;
            const String payload = String(packet.hex).substring(message.payloadOffset * 2, (message.payloadOffset + message.payloadLength) * 2);
            Serial.printf("PAYLOAD encrypted=%s length=%u bytes=%s\n", message.encrypted ? "yes" : "no", message.payloadLength, payload.c_str());
        }
    }
    digitalWrite(2, LOW);
    delay(1);
}
