#pragma once
#include <Arduino.h>
namespace cfg {
// TRACE AGAINST FINAL PCB NETLIST BEFORE FLASHING. Inferred from screenshot.
constexpr int PIN_SCLK=18, PIN_MISO=17, PIN_MOSI=16, PIN_CS=5;
constexpr int PIN_DRDY=19, PIN_CLKIN=4, PIN_RESET=26;
constexpr uint32_t REPORT_MS=1000;
constexpr uint32_t DRDY_TIMEOUT_US=50000;
// Set TRUE only after shunt, amplifier gains, offsets and ADC range are verified.
constexpr bool CALIBRATED=false;
// Deliberately NAN: unknown hardware parameters must not turn into plausible currents.
constexpr float SHUNT_OHM[3]={NAN,NAN,NAN};
constexpr float AMP_GAIN[3]={NAN,NAN,NAN};
constexpr float ZERO_V[3]={NAN,NAN,NAN};
// ADS131M04 internal nominal reference (check reference setting on actual PCB).
constexpr float ADC_VREF=1.2f;
constexpr float ADC_PGA_GAIN=1.0f;
// Phase A/B/C -> ADS channels 0/1/2; ch3 reserved for Vbus.
}
