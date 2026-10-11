# Changelog

All notable changes to ESP-KVM are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Project scaffold: PlatformIO dual-environment build (`dongle` + `controller`) on the
  pioarduino platform (arduino-esp32 3.x).
- Shared ESP-NOW wire protocol (`lib/espkvm_proto`): versioned packet, key-combo /
  type-string / mouse / media command types.
- Phase 1 coexistence spike: T-Dongle S3 runs a USB-HID keyboard and ESP-NOW together
  and types live tx/rx counters on a BOOT-button press.
- CoreS3 boot splash via M5Unified.
