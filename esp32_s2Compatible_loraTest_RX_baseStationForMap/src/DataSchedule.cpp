#include "DataSchedule.h"

namespace sidewalk {

namespace {
constexpr uint8_t kDataChannels = 64;

uint16_t advanceLfsr(uint16_t state) {
    uint16_t feedback = (state ^ (state >> 2) ^ (state >> 3) ^ (state >> 5)) & 1;
    return static_cast<uint16_t>((state >> 1) | (feedback << 15));
}

void shuffleChannels(uint32_t seed, uint32_t group, uint8_t* channels) {
    uint16_t state = advanceLfsr(static_cast<uint16_t>((seed >> 16) ^ seed ^ group));
    if (state == 0) state = 0xACE1;
    for (uint8_t index = 0; index < ChannelCount; ++index) channels[index] = index;
    for (uint8_t index = 0; index < ChannelCount; ++index) {
        const uint8_t other = state % ChannelCount;
        const uint8_t value = channels[index];
        channels[index] = channels[other];
        channels[other] = value;
        state = advanceLfsr(state);
    }
}

bool matchesSchedule(uint8_t slot, uint8_t offset, uint8_t periodicity) {
    return periodicity != 0 && slot >= offset && (slot - offset) % periodicity == 0;
}
} // namespace

bool dataChannel(uint32_t seed, uint16_t sequence, uint8_t slot, uint8_t& channel) {
    if (slot < 3 || slot >= SlotsPerBeacon - 1) return false;
    bool excluded[ChannelCount] = {};
    uint8_t excludedCount = 0;
    for (int8_t neighbor = -2; neighbor <= 2; ++neighbor) {
        const uint8_t beacon = beaconChannel(seed, static_cast<uint16_t>(sequence + neighbor));
        if (!excluded[beacon]) {
            excluded[beacon] = true;
            ++excludedCount;
        }
    }
    // The published plan requires exactly 64 survivors. Do not invent a
    // truncation rule when adjacent beacon permutations repeat a channel.
    if (excludedCount != ChannelCount - kDataChannels) return false;

    const uint32_t absoluteSlot = static_cast<uint32_t>(sequence) * SlotsPerBeacon + slot;
    uint8_t channels[ChannelCount];
    shuffleChannels(seed, absoluteSlot / kDataChannels, channels);
    const uint8_t wanted = absoluteSlot % kDataChannels;
    uint8_t survivor = 0;
    for (uint8_t index = 0; index < ChannelCount; ++index) {
        if (excluded[channels[index]]) continue;
        if (survivor == wanted) {
            channel = channels[index];
            return true;
        }
        ++survivor;
    }
    return false;
}

uint8_t commonSlotDirections(const Beacon& beacon, uint8_t slot) {
    if (!beacon.hasSchedule || slot < 3 || slot >= SlotsPerBeacon - 1) return 0;
    uint8_t directions = 0;
    if (matchesSchedule(slot, beacon.csUlOffset, beacon.csUlPeriodicity)) directions |= CommonUplink;
    if (matchesSchedule(slot, beacon.csDlOffset, beacon.csDlPeriodicity)) directions |= CommonDownlink;
    return directions;
}

bool opportunityForSlot(const Beacon& beacon, uint32_t lastSfdUs,
                        uint16_t sequence, uint8_t slot, DataOpportunity& result,
                        int32_t phase) {
    uint8_t channel = 0;
    if (!dataChannel(beacon.seed, sequence, slot, channel)) return false;
    const int16_t intervals = static_cast<int16_t>(sequence - beacon.sequence);
    const int64_t offsetUs = static_cast<int64_t>(intervals) * BeaconIntervalUs +
                             static_cast<int64_t>(slot) * SlotIntervalUs + phase;
    if (offsetUs < -2147483647LL || offsetUs > 2147483647LL) return false;
    result.beaconSequence = sequence;
    result.slot = slot;
    result.directions = commonSlotDirections(beacon, slot);
    result.frequencyHz = channelFrequency(channel);
    result.targetUs = lastSfdUs + static_cast<uint32_t>(offsetUs);
    result.openUs = result.targetUs - DataWindowLeadUs;
    result.closeUs = result.targetUs + DataWindowTailUs;
    result.timingEstimated = true;
    return true;
}

bool nextCommonOpportunity(const Beacon& beacon, uint32_t lastSfdUs,
                           uint32_t nowUs, DataOpportunity& result, int32_t phase) {
    if (!beacon.hasSchedule) return false;
    bool found = false;
    int32_t bestDelta = 0;
    const int64_t sinceSlotZero = static_cast<int32_t>(nowUs - lastSfdUs) - static_cast<int64_t>(phase);
    const uint32_t firstInterval = sinceSlotZero > 0 ? sinceSlotZero / BeaconIntervalUs : 0;
    for (uint32_t interval = firstInterval; interval < firstInterval + 3; ++interval) {
        const uint16_t sequence = static_cast<uint16_t>(beacon.sequence + interval);
        for (uint8_t slot = 3; slot < SlotsPerBeacon - 1; ++slot) {
            if (commonSlotDirections(beacon, slot) == 0) continue;
            DataOpportunity candidate;
            if (!opportunityForSlot(beacon, lastSfdUs, sequence, slot, candidate, phase)) continue;
            if (static_cast<int32_t>(candidate.closeUs - nowUs) < 0) continue;
            const int32_t delta = static_cast<int32_t>(candidate.targetUs - nowUs);
            if (!found || delta < bestDelta) {
                result = candidate;
                bestDelta = delta;
                found = true;
            }
        }
    }
    return found;
}

bool dataScheduleSelfTest() {
    // Independently generated from the published data-hopping equations.
    // These validate the published algorithm, not over-the-air data reception.
    struct Vector {
        uint32_t seed;
        uint16_t sequence;
        uint8_t slot;
        uint8_t channel;
    };
    const Vector vectors[] = {
        {0x12345678, 11, 3, 52}, {0x12345678, 11, 8, 43},
        {0x12345678, 11, 9, 10}, {0x12345678, 11, 58, 18},
        {0x12345678, 11, 158, 52}, {0, 11, 8, 41}, {0, 12, 8, 31},
        {0x12345678, 65535, 8, 16}, {0x12345678, 68, 8, 1},
        {0x12345678, 69, 8, 33}
    };
    uint8_t channel = 0;
    for (const Vector& vector : vectors) {
        if (!dataChannel(vector.seed, vector.sequence, vector.slot, channel) ||
            channel != vector.channel) return false;
    }
    if (dataChannel(1, 69, 8, channel)) return false; // Duplicate excluded channels.

    Beacon beacon;
    beacon.seed = 0x12345678;
    beacon.sequence = 11;
    beacon.hasSchedule = true;
    beacon.csUlOffset = 8;
    beacon.csUlPeriodicity = 50;
    beacon.csDlOffset = 9;
    beacon.csDlPeriodicity = 50;
    if (commonSlotDirections(beacon, 8) != CommonUplink ||
        commonSlotDirections(beacon, 58) != CommonUplink ||
        commonSlotDirections(beacon, 9) != CommonDownlink ||
        commonSlotDirections(beacon, 159) != 0 ||
        commonSlotDirections(beacon, 7) != 0) return false;

    DataOpportunity slot;
    constexpr uint32_t anchor = 0xFFFF0000U;
    if (!opportunityForSlot(beacon, anchor, 12, 8, slot)) return false;
    if (slot.targetUs != anchor + BeaconIntervalUs + 8 * SlotIntervalUs - 140000 ||
        slot.openUs != slot.targetUs - DataWindowLeadUs ||
        slot.closeUs != slot.targetUs + DataWindowTailUs) return false;
    if (!nextCommonOpportunity(beacon, anchor, anchor, slot) || slot.slot != 8 ||
        slot.beaconSequence != 11) return false;
    const uint32_t later = anchor + 5 * BeaconIntervalUs;
    if (!nextCommonOpportunity(beacon, anchor, later, slot) || slot.slot != 8 ||
        slot.beaconSequence != 16) return false;
    beacon.hasSchedule = false;
    if (nextCommonOpportunity(beacon, anchor, anchor, slot)) return false;
    beacon.hasSchedule = true;
    beacon.csUlPeriodicity = 0;
    beacon.csDlPeriodicity = 0;
    if (nextCommonOpportunity(beacon, anchor, anchor, slot)) return false;

    if (dataChannel(beacon.seed, 11, 2, channel) ||
        dataChannel(beacon.seed, 11, 159, channel)) return false;
    for (uint8_t slotNumber = 3; slotNumber < 159; ++slotNumber) {
        if (!dataChannel(beacon.seed, 11, slotNumber, channel) || channel >= ChannelCount) return false;
        for (int8_t neighbor = -2; neighbor <= 2; ++neighbor) {
            if (channel == beaconChannel(beacon.seed, static_cast<uint16_t>(11 + neighbor))) return false;
        }
    }
    return true;
}

} // namespace sidewalk
