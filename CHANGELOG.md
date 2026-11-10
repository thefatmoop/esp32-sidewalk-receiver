# Changelog

## v0.1.1 — 2026-11-09

Public source release documenting the validated receiver functionality.

- Lead with live-validated gateway discovery, CRC-checked reception, hop prediction, multi-gateway following, and dashboard operation.
- Record validation results of five gateway/PAN identifiers, 77 CRC-valid frames, independent channel comparisons, and zero FIFO overflows.
- Describe the remaining live encrypted application-payload capture check specifically within the data feature's validation notes.

## v0.1.0 — 2026-11-09

Initial source release for ESP32 and SX1276.

- Passive 50 kb/s FSK reception with software PHY header, PN9, CRC16, and CRC32 validation.
- Gateway beacon parsing, multi-gateway channel following, and scanning between scheduled opportunities.
- Public shared-slot listening and sampling of other data-hopping slots.
- Bounded link/network header parsing with ciphertext, authentication-byte, and security-counter extraction for supported layouts.
- Local web dashboard with separate gateway data panels, timestamps, RF statistics, full frame hex, and receiver controls.
- Synthetic public fixtures and independent protocol test vectors.
- Portable PlatformIO build configuration and an ignored local Wi-Fi credential file.

Gateway beacon following has been verified on received signals. Live encrypted application-payload capture remains unverified. Data timing is estimated, schedule-based gateway association is labeled, and one radio can miss overlapping transmissions.

This release distributes source. Configure your own Wi-Fi credentials and build the firmware using the README instructions.
