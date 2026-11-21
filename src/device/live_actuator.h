// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

namespace heliograph::v51 {

// A driver supplies this interface only after its device and control path are
// qualified. Power values use the EMS convention, including readback.
class LiveActuator {
public:
    virtual ~LiveActuator() = default;
    virtual bool hasControl() const = 0;
    virtual std::optional<int> writeAndReadBack(int watts) = 0;
    // Recheck ownership before releasing it. A failed release leaves the
    // output state unknown and stops all setpoint renewal.
    virtual bool release() = 0;
};

}
