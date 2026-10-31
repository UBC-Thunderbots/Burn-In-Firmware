# Thunderbots Burn-In Fixture — ESP32 Firmware Design Documentation

**Version:** 0.2 — Initial Current Acquisition and RMS Processing  
**Platform:** ESP32-DEVKITC-32E / ESP32-WROOM-32  
**ADC:** Texas Instruments ADS131M04  
**Framework:** Arduino (PlatformIO)  
**Driver:** Imperial College Rocketry ADS131M04 Arduino Library - [Access them here](https://github.com/icl-rocketry/ADS131M04-Lib)

**Status:** Bench prototype — build and hardware validation pending

## 1. Firmware Overview

The firmware is designed to independently measure the three-phase currents produced by the Thunderbots motor driver during burn-in testing.

An ESP32 microcontroller communicates with an ADS131M04 four-channel, 24-bit simultaneous-sampling analog-to-digital converter over SPI.

The ADC acquires conditioned analog signals from three current-sense circuits, each consisting of an inline shunt resistor and current-sense amplifier.

The firmware detects ADC data-ready events, retrieves the conversion results, converts them into phase-current measurements using configurable calibration parameters, and accumulates samples to calculate RMS current.

Measurement statistics and acquisition diagnostics are periodically transmitted through USB Serial.

The fourth ADC channel is reserved for future DC bus voltage measurement.

## 2. ADC Initialization and SPI Communication

The firmware initializes the ESP32 SPI interface using the custom GPIO assignments defined in `config.h`.

The Imperial College Rocketry ADS131M04 driver is instantiated using the chip-select pin, ADC clock-output pin and ESP32 SPI peripheral.

The `adc.begin()` function initializes the driver's communication interface and generates the ADC's external clock using the ESP32 LEDC peripheral.

During startup, the firmware reads three ADC configuration registers:

- `ID` — Device identification
- `MODE` — ADC operating configuration
- `CLOCK` — Clock and channel configuration

These readings provide preliminary communication diagnostics. Suspicious register responses cause the firmware to stop acquisition.

**Design rationale:** Register-based communication checks help identify initialization and SPI faults before measurement processing begins.
 

## 3. DRDY-Based Acquisition

The ADS131M04 provides an active-low Data Ready (DRDY) signal indicating that conversion data is available.

The ESP32 continuously polls DRDY and detects falling edges by comparing the current GPIO state against the previous state.

Upon detecting a falling edge, the firmware calls `adc.rawChannels()` to obtain the four ADC conversion results through SPI.

The results are stored as signed 32-bit integers containing the decoded 24-bit ADC values.

**Channel allocation:**

| ADC Channel | Assigned Measurement |
|---|---|
| CH0 | Phase A current |
| CH1 | Phase B current |
| CH2 | Phase C current |
| CH3 | DC bus voltage — future implementation |

The ADS131M04 simultaneously samples all four channels, avoiding the interchannel sampling delays associated with multiplexed ADCs.

**Design rationale:** DRDY-controlled acquisition allows the ESP32 to retrieve ADC readings when conversion data becomes available instead of sampling according to an unrelated software timer.

**Limitation:** The current polling implementation does not guarantee the capture of every conversion. Interrupt-driven acquisition and buffering should be evaluated during performance validation.

## 4. ADC Voltage Conversion

The ADS131M04 provides signed 24-bit conversion results.

The firmware converts each digital code into an estimated differential input voltage using the nominal ADC reference voltage and configured programmable gain.


$$
V_{\mathrm{ADC}}=\frac{\mathrm{ADC\ Code}}{2^{23}}\cdot\frac{V_{\mathrm{REF}}}{G_{\mathrm{ADC}}}
$$
  

Where:

- \(V_{\mathrm{REF}}\) is the ADC reference voltage.
- \(G_{\mathrm{ADC}}\) is the internal ADC programmable gain.
- The denominator \(2^{23}\) corresponds to the signed 24-bit conversion scale.

The reference voltage and gain are defined in `config.h` and must correspond to the physical ADC configuration.

## 5. Current-Sense Conversion and Calibration

Each phase current is measured using an inline shunt resistor and current-sense amplifier.

The conditioned ADC voltage is converted into phase current using:

$$
I_{\mathrm{phase}}=\frac{V_{\mathrm{ADC}}-V_{\mathrm{offset}}}{G_{\mathrm{amp}}R_{\mathrm{shunt}}}
$$

Where:

- \(V_{\mathrm{offset}}\) is the measured zero-current differential offset.
- \(G_{\mathrm{amp}}\) is the external current-sense amplifier gain.
- \(R_{\mathrm{shunt}}\) is the shunt resistance.

The firmware uses `readyCalibration()` during startup to verify that the required calibration parameters are enabled, finite and physically plausible.

Calibration readiness is stored in `calibrationReady`, avoiding repeated checks during every acquisition.

If calibration is unavailable, the firmware continues acquiring raw ADC measurements but does not calculate phase currents.

**Design rationale:** This prevents missing or invalid configuration values from propagating into reported electrical measurements.

The software checks do not replace physical calibration of the measurement chain.

## 6. RMS Current Calculation

The firmware uses an `Accumulator` structure to calculate current statistics without storing complete waveform histories.

For each accepted current sample, the firmware adds its square to a running sum.

The RMS current is then calculated over the reporting interval:

$$
I_{\mathrm{RMS}}=\sqrt{\frac{1}{N}\sum_{n=1}^{N}I_n^2}
$$

A separate `validCount` counter tracks the number of accepted samples for each phase.

The accumulator also records the minimum and maximum observed current values.

**Design rationale:** Sample-based RMS supports nonsinusoidal waveforms without requiring a peak-current estimate or assuming a perfect sinusoidal shape.

The method assumes sufficiently representative, uniformly timed samples and adequate analog/digital measurement bandwidth.

## 7. Measurement Diagnostics

The firmware tracks several diagnostic conditions.

| Diagnostic | Description |
|---|---|
| `count` | Number of acquired frames processed |
| `drdySeen` | Number of detected DRDY falling edges |
| `timeouts` | Number of extended intervals without a detected DRDY event |
| `clipping` | Number of samples reaching the signed 24-bit ADC code limits |
| `invalid` | Number of calculated current values rejected for being nonfinite |
| `validCount[ch]` | Number of accepted current samples for each phase |

Full-scale endpoint readings are excluded from RMS accumulation and recorded as potential clipping events.

These events indicate possible saturation but do not necessarily prove that the physical input exceeded the ADC's permissible range.

The timeout monitor identifies extended gaps in acquisition, but the current implementation cannot detect every individual dropped conversion or corrupted SPI frame.

## 8. Serial Reporting

The firmware periodically calls `printReport()` according to the reporting interval configured in `config.h`.

The report contains:

- Acquired frame count
- DRDY events
- Timeout and clipping counters
- Invalid numerical measurement count
- RMS current for phases A, B and C
- Minimum and maximum phase currents
- Accepted sample counts
- Latest four raw ADC conversion results

After printing, the accumulator is reset for the next measurement interval.

**Design rationale:** Periodic reporting reduces Serial output overhead compared with printing individual samples and provides an updated summary of the fixture's measurements.

## 9. Firmware Architecture

The primary functions are:

| Function | Responsibility |
|---|---|
| `setup()` | ESP32 and ADC initialization, communication checks |
| `loop()` | DRDY polling, acquisition scheduling, timeout detection and reporting |
| `readyCalibration()` | Validate calibration configuration |
| `adcVolts()` | Convert signed ADC codes to differential voltage |
| `adcCurrent()` | Convert ADC voltage to phase current |
| `processSamples()` | Calculate statistics and process measurement validity |
| `printReport()` | Output periodic measurements and reset statistics |


## 10. Remaining Development and Validation/Test plan

The following tasks remain before the firmware can be considered validated for burn-in measurements:

1. Compile against the selected Arduino-ESP32 framework and the installed ADS131M04 driver.
2. Verify SPI communication and ADC master-clock generation using laboratory instruments.
4. Measure DRDY timing and verify achievable acquisition throughput.
5. Verify RMS measurements using known input signals.
6. Flag reporting intervals with clipping or acquisition errors as unsuitable for automated pass/fail decisions.
7. Implement calibrated Vbus measurement through the fourth channel.
8. Verify the complete measurement chain against the specified ±10 mA accuracy requirement.

## 11. Conclusion

The initial ESP32 firmware establishes the software architecture for acquiring and processing three-phase current measurements using the ADS131M04.

The current implementation is suitable as a foundation for bench bring-up and further firmware development.

The next primary challenge will be validating acquisition reliability and physical measurement accuracy before deploying the fixture for live motor-driver burn-in testing.
