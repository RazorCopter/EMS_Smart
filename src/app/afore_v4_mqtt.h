// SPDX-License-Identifier: MIT
//
// Afore V4 Shadow Mode MQTT Discovery and Telemetry Publisher (CP3).
//
// Generates Home Assistant MQTT Discovery messages for Afore AF5K-SL with:
//   - Node ID: afore_v4 (configurable)
//   - Device metadata: Manufacturer "Afore", Model "AF5K-SL (Shadow Mode)"
//   - VERIFIED measurements as enabled entities
//   - UNVERIFIED measurements as diagnostic/disabled entities
//   - Shadow controller state entities (mode, quality_summary, availability)
//   - Quality attributes attached to each measurement

#pragma once

#include <string>
#include <vector>

#include "afore_telemetry_engine.h"
#include "device/bridge_info.h"
#include "outputs/mqtt/home_assistant_discovery.h"

namespace heliograph {

/// Afore V4-specific MQTT discovery and payload builder.
///
/// Wraps the generic Home Assistant discovery with Afore-specific conventions:
///   - Node ID defaults to "afore_v4" but configurable for multi-device setups
///   - Device class and state_class inferred from measurement taxonomy
///   - Quality metadata published as JSON attributes
///   - Shadow controller entities for Phase 0 observability
class AforeV4MqttPublisher {
public:
    struct Config {
        std::string nodeId = "afore_v4";
        std::string discoveryPrefix = "homeassistant";
        std::string baseTopic;  ///< e.g. "heliograph/afore_shadow"
        bool enableUnverified = false;  ///< Publish UNVERIFIED measurements (diagnostic)
    };
    
    AforeV4MqttPublisher(const Config& config);
    
    /// Builds Home Assistant discovery messages for all Afore measurements.
    /// Returns vector of discovery entities to publish (config topic + payload).
    std::vector<mqtt::DiscoveryEntity> buildDiscoveryMessages();
    
    /// Builds telemetry state payload with quality attributes.
    /// Format:
    ///   {
    ///     "battery_soc": 80,
    ///     "battery_power": 200,
    ///     "pv_power": 1200,
    ///     ...
    ///     "_quality": {
    ///       "battery_soc": "good",
    ///       "battery_power": "good",
    ///       "pv_power": "degraded",
    ///       ...
    ///     },
    ///     "_age_ms": {
    ///       "battery_soc": 1250,
    ///       ...
    ///     }
    ///   }
    std::string buildTelemetryPayload(const AforeTelemetryEngine& engine);
    
    /// Builds shadow controller state payload (Phase 0 observability).
    /// Format:
    ///   {
    ///     "mode": "shadow",
    ///     "quality_summary": "good",  // worst quality across all samples
    ///     "actionable_measurements": 12,
    ///     "stale_measurements": 2,
    ///     "invalid_measurements": 0,
    ///     "success_rate": 98.5,
    ///     "last_poll_ms": 1234567890
    ///   }
    std::string buildShadowStatePayload(const AforeTelemetryEngine& engine, uint64_t nowMs);
    
    /// Builds availability (LWT) message: "online" or "offline"
    std::string buildAvailabilityPayload(bool online) const;
    
    const std::string& getStateTopic() const { return stateTopic_; }
    const std::string& getShadowStateTopic() const { return shadowStateTopic_; }
    const std::string& getAvailabilityTopic() const { return availabilityTopic_; }

private:
    Config config_;
    std::string stateTopic_;
    std::string shadowStateTopic_;
    std::string availabilityTopic_;
    
    mqtt::DiscoveryEntity buildSensorEntity(const std::string& measurementId,
                                            const std::string& name,
                                            const std::string& deviceClass,
                                            const std::string& stateClass,
                                            const std::string& unit,
                                            bool verified);
    
    mqtt::DiscoveryEntity buildShadowSensorEntity(const std::string& entityId,
                                                  const std::string& name,
                                                  const std::string& valueTemplate);
};

}  // namespace heliograph
