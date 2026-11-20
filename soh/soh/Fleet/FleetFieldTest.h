#pragma once

#include <optional>
#include <string>
#include <nlohmann/json.hpp>

// Test drives for fleet-test: requests are queued by the host and run at the end of a game frame.
std::optional<std::string> FleetFieldTest_Request(const std::string& request, const nlohmann::json& input);
void FleetFieldTest_FrameEnd();
