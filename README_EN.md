# ESPHome IRHVAC for ESP8266 and ESP32

[العربية](README.md) | [English](README_EN.md)

Standalone ESPHome configurations that send and receive air-conditioner IR
commands using [`IRremoteESP8266`](https://github.com/crankyoldgit/IRremoteESP8266)
version `2.9.0`. The configured fork adds a small, protocol-independent timing
bridge: `IRremoteESP8266` still encodes the A/C command, while ESPHome emits the
resulting envelope. MQTT commands and received-result JSON use the Tasmota
`IRHVAC`/`IrReceived` layout.

> Compatibility here specifically covers the MQTT `IRHVAC` command and the
> received `IrReceived` result. This project does not turn ESPHome into Tasmota
> or implement unrelated Tasmota commands.

## Choose one file

Every YAML file is completely standalone and can be copied by itself into the
ESPHome dashboard:

| Chip | File | Default board |
|---|---|---|
| ESP8266 | `ir-blaster-esp8266.yaml` | D1 mini |
| ESP32 | `ir-blaster-esp32.yaml` | ESP32 DevKit |
| ESP32-C3 | `ir-blaster-esp32-c3.yaml` | ESP32-C3 DevKitM-1 |
| ESP32-S2 | `ir-blaster-esp32-s2.yaml` | ESP32-S2 Saola-1 |
| ESP32-S3 | `ir-blaster-esp32-s3.yaml` | ESP32-S3 DevKitC-1 |

The ESP32 configurations use the Arduino framework because the library targets
that framework. ESP32-C2/C5/C6/C61/H2/P4 are not included because ESPHome does
not provide the Arduino framework for those chips.

## Setup

1. Create a new device in the ESPHome dashboard.
2. Replace its YAML contents with the file matching your chip.
3. Change both IR pin numbers at the top of the file:

   ```yaml
   tx_gpio: "4"
   rx_gpio: "14"
   ```

   Enter actual GPIO numbers only. For example, `D2` on a D1 mini is `GPIO4`
   and `D5` is `GPIO14`. ESP32-C3/S2/S3 files use `GPIO5` as the default
   receiver pin.

4. Add your settings to ESPHome's `secrets.yaml`:

   ```yaml
   wifi_ssid: "YOUR_WIFI_SSID"
   wifi_password: "YOUR_WIFI_PASSWORD"
   mqtt_broker: "192.168.1.10"
   mqtt_username: "YOUR_MQTT_USERNAME"
   mqtt_password: "YOUR_MQTT_PASSWORD"
   ```

5. Install the firmware on the device.

Do not add another ESPHome `remote_transmitter` or `remote_receiver` on these
GPIOs; the complete YAML already configures the required transmitter. On
ESP32, ESPHome uses the hardware RMT peripheral to emit the timing envelope,
avoiding software-carrier jitter caused by Wi-Fi and framework interrupts.
`IRremoteESP8266` owns the receive pin directly. Use a suitable transistor to
drive the infrared LED; do not connect a high-current LED directly to a GPIO.
Connect a 3.3 V demodulating IR receiver module to `rx_gpio`.

## What changed in the libraries?

The project uses two repositories in addition to ESPHome:

- The [`IRremoteESP8266-ESPHome-LibreTiny`](https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny)
  branch is based on the official `IRremoteESP8266 2.9.0` release. Commit
  `04b20e7` is the library revision tested with this project.
- `src/irhvac_esp_controller.h` in this repository parses Tasmota-style JSON,
  creates `stdAc::state_t`, invokes `IRac`, and connects transmission and
  reception to MQTT and the ESPHome log.

No A/C protocol encoder was modified. Files such as `IRac.cpp`, `ir_Gree.cpp`,
and `ir_Kelvinator.cpp` remain identical to upstream. The source difference
from `v2.9.0` is limited to five files (167 insertions and two deletions):

- `IRsend.h` and `IRsend.cpp` gained optional callbacks for mark, space,
  carrier frequency, and duty cycle. When enabled, the library hands the
  encoded envelope to ESPHome. With no callbacks installed, the original
  upstream behavior is unchanged.
- `irremote_esphome_bridge.h` connects those callbacks to
  `RemoteTransmitData`. ESP32/C3/S2/S3 therefore use the hardware RMT
  peripheral, while ESP8266 uses ESPHome's software transmitter.
- `IRrecv::decodeRaw()` and `LIBRETINY` build guards let the BK7231N project
  pass ESPHome captures to the decoder. This code is inactive on native ESP;
  IRremoteESP8266 still owns reception directly on ESP8266 and ESP32 here.

This is why a repository named `LibreTiny` appears in an ESP32 build: the same
branch carries the protocol-independent transmit timing bridge needed on ESP.
The unmodified official library is not currently a drop-in replacement. It
lacks the bridge and callbacks and would return ESP32-C3 to software carrier
generation, which produced malformed pulses and partial `UNKNOWN` frames when
Wi-Fi and framework interrupts intervened.

### Pinning and updates

The YAML files track `#esphome-libretiny` to receive project updates. However,
PlatformIO caches Git dependencies and does not fetch the newest commit on
every build. After the branch changes, use **Clean Build Files** in ESPHome and
build again.

For a reproducible build, pin the revisions proven by the end-to-end test:

```yaml
ir_library_source: https://github.com/zain1144/IRremoteESP8266-ESPHome-LibreTiny.git#04b20e7
ir_controller_source: https://github.com/zain1144/ESPHome-IRHVAC-ESP.git#db8cabb
```

When a newer IRremoteESP8266 release becomes available, merge or rebase
upstream into the compatibility branch, reapply the small bridge if needed,
then test both transmission and reception. Do not switch directly to the
official source first: these additions have not been merged upstream.

## MQTT

The device listens on both topics below:

```text
cmnd/ir-blaster/IRHVAC
cmnd/ir-blaster/irhvac
```

The main payload format matches the input accepted by Tasmota:

```json
{
  "Vendor": "KELVINATOR",
  "Model": -1,
  "Command": "Control",
  "Mode": "Cool",
  "Power": "Off",
  "Celsius": "On",
  "Temp": 25,
  "FanSpeed": "Min",
  "SwingV": "Off",
  "SwingH": "Off",
  "Quiet": "Off",
  "Turbo": "Off",
  "Econo": "Off",
  "Light": "Off",
  "Filter": "Off",
  "Clean": "On",
  "Beep": "Off",
  "Sleep": -1,
  "iFeel": "Off",
  "SensorTemp": null
}
```

The wrapped format is accepted as well:

```json
{"IRHVAC":{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":25}}
```

After transmitting, the device publishes the state it used to:

```text
stat/ir-blaster/RESULT
```

When the physical IR receiver detects any supported code, the device publishes
the Tasmota-style result to:

```text
tele/ir-blaster/RESULT
```

For a decoded HVAC frame, the payload includes its complete normalized state:

```json
{"IrReceived":{"Protocol":"KELVINATOR","Bits":128,"Data":"0x...","Repeat":0,"IRHVAC":{"Vendor":"KELVINATOR","Model":-1,"Command":"Control","Mode":"Cool","Power":"On","Celsius":"On","Temp":24,"FanSpeed":"Min","SwingV":"Auto","SwingH":"Auto","Quiet":"Off","Turbo":"Off","Econo":"Off","Light":"Off","Filter":"Off","Clean":"On","Beep":"Off","Sleep":-1,"iFeel":"Off","SensorTemp":null}}}
```

The same complete `IRHVAC` object is written to the ESPHome log. Recognized
non-HVAC protocols and unknown captures are still published with protocol,
bit count, data or hash, and repeat information.

Example using Mosquitto:

```bash
mosquitto_pub -h 192.168.1.10 \
  -t cmnd/ir-blaster/IRHVAC \
  -m '{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":25}'
```

If you change the device name or any topic substitution at the top of the
file, configure your Home Assistant integration to use the same topic.

## Command fields

The adapter accepts the following fields:

`Vendor`, `Model`, `Command`, `Mode`, `Power`, `Celsius`, `Temp`,
`FanSpeed`, `SwingV`, `SwingH`, `Quiet`, `Turbo`, `Econo`, `Light`,
`Filter`, `Clean`, `Beep`, `Sleep`, `iFeel`, and `SensorTemp`.

Feature availability depends on the air conditioner's protocol. A field being
accepted by the library does not mean every device supports that feature. The
available vendor and protocol names are those supported by `IRac` in
`IRremoteESP8266 2.9.0`. The Kelvinator payload above is only an example; the
configuration is not limited to Kelvinator units.

## Verification

The configurations were checked with ESPHome `2026.8.2`. All ESP8266, ESP32,
ESP32-C3, ESP32-S2, and ESP32-S3 YAML files pass configuration validation.
Full compilation and firmware generation also completed successfully for
ESP8266 and ESP32-C3. These cover both ESPHome transmit paths used here: the
ESP8266 software transmitter and ESP32's hardware RMT transmitter.

The ESP32-C3 build was also installed by OTA and tested end to end on real
hardware. A Tasmota receiver decoded the transmitted signal as a complete
128-bit `KELVINATOR` HVAC state and as a complete 64-bit `GREE` HVAC state.
ESP32, ESP32-S2, and ESP32-S3 passed configuration validation and use the same
RMT path, but were not tested on physical boards in this verification.

Every YAML file is standalone and uses neither `packages` nor `!include`. The
referenced fork is based on `IRremoteESP8266 2.9.0`; its timing-capture hooks
do not change any protocol encoder. A small adapter from this repository
converts MQTT and decoded states to the Tasmota JSON layout and sends the
captured envelope through ESPHome's platform transmitter. Real-world feature
compatibility still depends on the protocol, air-conditioner model,
transmitter circuit, and receiver hardware.
