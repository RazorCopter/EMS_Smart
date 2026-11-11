// SPDX-License-Identifier: MIT
// M3A Negative Compile Test
//
// This file MUST NOT compile when AFORE_SHADOW_BUILD=1
// Verifies that setForceReadOnly() is unavailable at compile time

#include <memory>
#include "../../src/transport/readonly_guard.h"
#include "../fake_lower_transport.h"

int main() {
    auto fake = std::make_unique<heliograph::FakeLowerTransport>();
    heliograph::ReadOnlyGuard guard(std::move(fake), 1);
    
    // This line MUST cause a compile error when AFORE_SHADOW_BUILD=1
    // Expected error: 'setForceReadOnly' is not a member of 'heliograph::ReadOnlyGuard'
    guard.setForceReadOnly(false);
    
    return 0;
}
