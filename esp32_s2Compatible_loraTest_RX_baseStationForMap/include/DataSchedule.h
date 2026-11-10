#pragma once

#include "Sidewalk.h"

namespace sidewalk {

constexpr uint32_t SlotIntervalUs = 63000;
constexpr uint16_t SlotsPerBeacon = 160;
constexpr uint32_t DataWindowLeadUs = 24000;
constexpr uint32_t DataWindowTailUs = 58000;
// Appendix B.1 specifies 140 ms of beacon preamble. Contention can prepend
// additional preamble before the target time. Keep this phase an estimate
// until CRC-valid data traffic independently confirms the slot alignment.
constexpr int32_t DefaultSlotZeroAfterSfdUs = -140000;
constexpr uint8_t CommonUplink = 1;
constexpr uint8_t CommonDownlink = 2;

struct DataOpportunity {
    uint16_t beaconSequence = 0;
    uint8_t slot = 0;
    uint8_t directions = 0; // CommonUplink/CommonDownlink, zero for other slots.
    uint32_t frequencyHz = 0;
    uint32_t targetUs = 0;
    uint32_t openUs = 0;
    uint32_t closeUs = 0;
    bool timingEstimated = true;
};

// Section 5.2.4.4. Returns false if slot is reserved or if the five excluded
// beacon channels are not distinct across a beacon permutation boundary.
bool dataChannel(uint32_t seed, uint16_t sequence, uint8_t slot, uint8_t& channel);
uint8_t commonSlotDirections(const Beacon& beacon, uint8_t slot);

// lastSfdUs is the measured SFD-start time for beacon.sequence. Arithmetic is
// wrap safe for a sequence distance below 32768 and a time horizon below 2^31us.
// This function also permits sampling slots absent from the public CS schedule.
bool opportunityForSlot(const Beacon& beacon, uint32_t lastSfdUs,
                        uint16_t sequence, uint8_t slot, DataOpportunity& result,
                        int32_t phase = DefaultSlotZeroAfterSfdUs);

// Looks through the current and next two superframes, extending from lastSfdUs.
// The caller owns its maximum tolerated age of the last beacon. A schedule is used only
// when explicitly present in a beacon. No endpoint-specific schedule is inferred.
bool nextCommonOpportunity(const Beacon& beacon, uint32_t lastSfdUs,
                           uint32_t nowUs, DataOpportunity& result,
                           int32_t phase = DefaultSlotZeroAfterSfdUs);

bool dataScheduleSelfTest();

} // namespace sidewalk
