#pragma once

namespace scorebot {

// Game-link GATT service shared by player boards and the LED-wall display.
constexpr char kGameServiceUuid[] = "c6a8619e-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kIdentityUuid[] = "c6a8619f-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kUplinkUuid[] = "c6a861a0-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kDownlinkUuid[] = "c6a861a1-2f9d-46bc-9a23-bb9c89a519be";
constexpr char kProtocolUuid[] = "c6a861a2-2f9d-46bc-9a23-bb9c89a519be";

}  // namespace scorebot
