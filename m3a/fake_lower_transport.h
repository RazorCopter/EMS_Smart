// SPDX-License-Identifier: MIT
// M3A Fake Lower Transport - Safety Core Validation

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "../src/transport/transport.h"

namespace heliograph {

/// Minimal fake transport for M3A safety core validation.
/// Records write() calls without actual I/O.
class FakeLowerTransport : public Transport {
public:
    FakeLowerTransport() = default;
    ~FakeLowerTransport() override = default;

    TransportType type() const override { return TransportType::Mock; }
    
    bool configure(const SerialProfile&) override { return true; }
    void flushInput() override {}
    
    size_t write(const uint8_t* data, size_t len) override {
        ++call_count_;
        total_bytes_ += len;
        last_frame_.assign(data, data + len);
        received_frames_.push_back(last_frame_);
        return len;
    }
    
    size_t read(uint8_t*, size_t, uint32_t) override { return 0; }
    uint64_t nowMs() const override { return 1000; }
    bool lock(uint32_t) override { return true; }
    void unlock() override {}

    // Accessors for test verification
    uint32_t call_count() const { return call_count_; }
    size_t total_bytes() const { return total_bytes_; }
    const std::vector<uint8_t>& last_frame() const { return last_frame_; }
    const std::vector<std::vector<uint8_t>>& received_frames() const { return received_frames_; }
    
    void reset() {
        call_count_ = 0;
        total_bytes_ = 0;
        last_frame_.clear();
        received_frames_.clear();
    }

private:
    uint32_t call_count_ = 0;
    size_t total_bytes_ = 0;
    std::vector<uint8_t> last_frame_;
    std::vector<std::vector<uint8_t>> received_frames_;
};

}  // namespace heliograph
