# ADR-001: OpenEMS Architectural Adaptation & License Boundary

## Context
OpenEMS is an open-source energy management framework licensed under the Eclipse Public License 2.0 (EPL-2.0 for Edge/Backend) and Affero General Public License (AGPL-3.0 for UI). Afore Autonomous EMS is licensed under the permissive MIT License and runs on low-power microcontrollers (ESP32-S3) as well as Linux single-board computers (Raspberry Pi Zero 2 W).

## Decision
1. **Clean-Room Algorithmic Adaptation**: We do not link, copy, or port OpenEMS Java source code or OSGi bundles. Instead, we adapt the mathematical formulas, state machine patterns, and control strategies (`Balancing`, `GridOptimizedCharge`, `LimitTotalDischarge`, `TimeOfUseTariff`, `FixStateOfCharge`) into pure, standalone C++17 implementations.
2. **License Compliance**: The clean-room C++17 codebase remains under the project's MIT License. We provide clear attribution to OpenEMS in documentation and citations.
3. **Embedded Optimization**: Where OpenEMS relies on a heavyweight Java VM and runtime reflection, our C++ core uses statically sized data structures, zero heap allocations in the fast 1 Hz cycle, and bounded memory buffers suitable for 512 KB microcontrollers.

## Consequences
- Full architectural independence without licensing contagion.
- Code is cross-compilable to both Linux host daemon and ESP32 firmware from a single unified codebase.
- Verified test suites can run under desktop CTest as well as on target embedded platforms.
