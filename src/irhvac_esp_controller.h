#pragma once

#include <IRac.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <cstdint>
#include <string>

#include "esphome/components/json/json_util.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// Tasmota-compatible IRHVAC command and receive adapter for native ESP chips.
// IRremoteESP8266 owns both GPIOs directly, as it does when used by Tasmota.
namespace esphome_irhvac_esp {

static const char *const TAG = "irhvac";
static IRac *ac = nullptr;
static IRrecv *receiver = nullptr;
static bool receiver_pullup = true;
static bool busy = false;
static bool transmitted_once = false;
static uint32_t last_transmit_ms = 0;
static bool received_state_valid = false;
static stdAc::state_t received_state;

static const uint32_t RECEIVE_ECHO_WINDOW_MS = 300;

struct SendResult {
  bool success{false};
  std::string error;
  std::string response_json;
};

struct ReceiveResult {
  bool publish{false};
  bool decoded{false};
  bool hvac{false};
  std::string response_json;
};

static bool read_bool(JsonObjectConst command, const char *key,
                      bool old_value) {
  JsonVariantConst value = command[key];
  if (value.isNull()) return old_value;
  if (value.is<bool>()) return value.as<bool>();
  if (value.is<const char *>())
    return IRac::strToBool(value.as<const char *>(), old_value);
  return value.as<int>() != 0;
}

static void write_model(JsonObject command, const stdAc::state_t &state) {
  const String model_name = modelToStr(state.protocol, state.model);
  if (model_name.length() != 0 && !model_name.equalsIgnoreCase("UNKNOWN"))
    command["Model"] = model_name.c_str();
  else
    command["Model"] = state.model;
}

static void write_state(JsonObject command, const stdAc::state_t &state) {
  command["Vendor"] = typeToString(state.protocol).c_str();
  write_model(command, state);
  command["Command"] = IRac::commandTypeToString(state.command).c_str();
  command["Mode"] = IRac::opmodeToString(state.mode).c_str();
  command["Power"] = state.power ? "On" : "Off";
  command["Celsius"] = state.celsius ? "On" : "Off";
  command["Temp"] = state.degrees;
  command["FanSpeed"] = IRac::fanspeedToString(state.fanspeed).c_str();
  command["SwingV"] = IRac::swingvToString(state.swingv).c_str();
  command["SwingH"] = IRac::swinghToString(state.swingh).c_str();
  command["Quiet"] = state.quiet ? "On" : "Off";
  command["Turbo"] = state.turbo ? "On" : "Off";
  command["Econo"] = state.econo ? "On" : "Off";
  command["Light"] = state.light ? "On" : "Off";
  command["Filter"] = state.filter ? "On" : "Off";
  command["Clean"] = state.clean ? "On" : "Off";
  command["Beep"] = state.beep ? "On" : "Off";
  command["Sleep"] = state.sleep;
  command["iFeel"] = state.iFeel ? "On" : "Off";
  if (state.sensorTemperature == kNoTempValue)
    command["SensorTemp"] = nullptr;
  else
    command["SensorTemp"] = state.sensorTemperature;
}

static std::string state_json(const bool success,
                              const std::string &error = "") {
  return esphome::json::build_json([&](JsonObject root) {
    JsonObject state = root["IRHVAC"].to<JsonObject>();
    if (ac != nullptr) write_state(state, ac->next);
    if (!success && !error.empty()) state["Error"] = error;
  });
}

static void setup(const uint16_t tx_pin, const uint16_t rx_pin,
                  const uint16_t receive_buffer_size = 1024,
                  const uint8_t receive_timeout_ms = 50,
                  const uint8_t receive_tolerance = 25,
                  const bool pullup = true) {
  if (ac == nullptr) ac = new IRac(tx_pin);
  if (receiver == nullptr) {
    receiver_pullup = pullup;
    // A save buffer lets capture resume while the decoded state and JSON are
    // being processed. The 1024-entry default handles long HVAC frames.
    receiver = new IRrecv(rx_pin, receive_buffer_size, receive_timeout_ms, true);
    receiver->setTolerance(receive_tolerance);
    receiver->enableIRIn(receiver_pullup);
  }
  ESP_LOGI(TAG,
           "Native IRremoteESP8266 ready: tx=GPIO%u rx=GPIO%u buffer=%u "
           "timeout=%ums tolerance=%u%%",
           static_cast<unsigned>(tx_pin), static_cast<unsigned>(rx_pin),
           static_cast<unsigned>(receive_buffer_size),
           static_cast<unsigned>(receive_timeout_ms),
           static_cast<unsigned>(receive_tolerance));
}

static SendResult send_object(JsonObjectConst input, const char *source) {
  SendResult result;
  JsonObjectConst command = input;
  if (!input["IRHVAC"].isNull())
    command = input["IRHVAC"].as<JsonObjectConst>();

  if (command.isNull()) {
    result.error = "Missing IRHVAC object";
    result.response_json = state_json(false, result.error);
    return result;
  }
  if (ac == nullptr) {
    result.error = "IR controller is not configured";
    result.response_json = state_json(false, result.error);
    return result;
  }
  if (busy) {
    result.error = "IR controller is busy";
    result.response_json = state_json(false, result.error);
    return result;
  }

  JsonVariantConst vendor_value = command["Vendor"];
  if (!vendor_value.isNull()) {
    const decode_type_t protocol =
        strToDecodeType(vendor_value.as<const char *>());
    if (protocol != ac->next.protocol) ac->next.model = -1;
    ac->next.protocol = protocol;
  }
  if (!IRac::isProtocolSupported(ac->next.protocol)) {
    const char *vendor = vendor_value.isNull()
                             ? "UNKNOWN"
                             : vendor_value.as<const char *>();
    result.error = std::string("Unsupported vendor: ") + vendor;
    result.response_json = state_json(false, result.error);
    ESP_LOGE(TAG, "%s", result.error.c_str());
    return result;
  }

  JsonVariantConst model = command["Model"];
  if (!model.isNull()) {
    ac->next.model = model.is<const char *>()
                         ? IRac::strToModel(model.as<const char *>(), -1)
                         : model.as<int16_t>();
  }
  if (!command["Command"].isNull())
    ac->next.command =
        IRac::strToCommandType(command["Command"].as<const char *>());
  if (!command["Mode"].isNull())
    ac->next.mode = IRac::strToOpmode(command["Mode"].as<const char *>());
  if (!command["Temp"].isNull())
    ac->next.degrees = command["Temp"].as<float>();
  if (!command["FanSpeed"].isNull())
    ac->next.fanspeed =
        IRac::strToFanspeed(command["FanSpeed"].as<const char *>());
  if (!command["SwingV"].isNull())
    ac->next.swingv =
        IRac::strToSwingV(command["SwingV"].as<const char *>());
  if (!command["SwingH"].isNull())
    ac->next.swingh =
        IRac::strToSwingH(command["SwingH"].as<const char *>());

  ac->next.power = read_bool(command, "Power", ac->next.power);
  ac->next.celsius = read_bool(command, "Celsius", ac->next.celsius);
  ac->next.quiet = read_bool(command, "Quiet", ac->next.quiet);
  ac->next.turbo = read_bool(command, "Turbo", ac->next.turbo);
  ac->next.econo = read_bool(command, "Econo", ac->next.econo);
  ac->next.light = read_bool(command, "Light", ac->next.light);
  ac->next.filter = read_bool(command, "Filter", ac->next.filter);
  ac->next.clean = read_bool(command, "Clean", ac->next.clean);
  ac->next.beep = read_bool(command, "Beep", ac->next.beep);
  ac->next.iFeel = read_bool(command, "iFeel", ac->next.iFeel);
  if (!command["Sleep"].isNull())
    ac->next.sleep = command["Sleep"].as<int16_t>();
  JsonVariantConst sensor_temp = command["SensorTemp"];
  if (!sensor_temp.isUnbound()) {
    if (sensor_temp.isNull())
      ac->next.sensorTemperature = kNoTempValue;
    else
      ac->next.sensorTemperature = sensor_temp.as<float>();
  }

  busy = true;
  if (receiver != nullptr) receiver->disableIRIn();
  result.success = ac->sendAc();
  if (result.success) {
    transmitted_once = true;
    last_transmit_ms = esphome::millis();
  } else {
    result.error = "IRac rejected the requested state";
  }
  if (receiver != nullptr) receiver->enableIRIn(receiver_pullup);
  busy = false;

  result.response_json = state_json(result.success, result.error);
  ESP_LOGI(TAG, "%s IRHVAC: vendor=%s result=%s", source,
           typeToString(ac->next.protocol).c_str(),
           result.success ? "SUCCESS" : "FAILED");
  return result;
}

static ReceiveResult receive() {
  ReceiveResult result;
  if (receiver == nullptr || busy) return result;

  decode_results decoded{};
  if (!receiver->decode(&decoded)) return result;
  // The save buffer used by setup() already rearms capture in decode().
  // Calling resume() is harmless and also keeps this correct if that changes.
  receiver->resume();

  result.decoded = true;
  const uint32_t now = esphome::millis();
  if (transmitted_once &&
      static_cast<uint32_t>(now - last_transmit_ms) <
          RECEIVE_ECHO_WINDOW_MS) {
    ESP_LOGD(TAG, "Ignoring receiver echo from the local transmitter");
    return result;
  }

  stdAc::state_t state;
  const stdAc::state_t *previous =
      received_state_valid &&
              received_state.protocol == decoded.decode_type
          ? &received_state
          : nullptr;
  result.hvac = IRAcUtils::decodeToState(&decoded, &state, previous);
  if (result.hvac) {
    received_state = state;
    received_state_valid = true;
    // Partial future MQTT commands continue from the physical remote state.
    if (ac != nullptr) ac->next = state;
  }

  const std::string protocol(typeToString(decoded.decode_type).c_str());
  const std::string value(resultToHexidecimal(&decoded).c_str());
  result.response_json = esphome::json::build_json([&](JsonObject root) {
    JsonObject received = root["IrReceived"].to<JsonObject>();
    received["Protocol"] = protocol;
    received["Bits"] = decoded.bits;
    if (decoded.decode_type == decode_type_t::UNKNOWN)
      received["Hash"] = value;
    else
      received["Data"] = value;
    received["Repeat"] = decoded.repeat ? 1 : 0;
    if (result.hvac)
      write_state(received["IRHVAC"].to<JsonObject>(), state);
  });
  result.publish = true;

  if (result.hvac) {
    const std::string hvac_json =
        esphome::json::build_json([&](JsonObject root) {
          write_state(root, state);
        });
    ESP_LOGI(TAG, "Received IRHVAC: %s", hvac_json.c_str());
  } else {
    ESP_LOGI(TAG, "Received IR: protocol=%s bits=%u hvac=no",
             protocol.c_str(), static_cast<unsigned>(decoded.bits));
  }
  return result;
}

}  // namespace esphome_irhvac_esp
