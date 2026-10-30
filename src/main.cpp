#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "ADS131M04.h"  // icl-rocketry/ADS131M04-Lib
#include "config.h"

SPIClass adcSPI(VSPI);
ADS131M04 adc(cfg::PIN_CS, cfg::PIN_CLKIN, &adcSPI);
int8_t channels[] = {0,1,2,3};
int32_t samples[4] = {};

struct Accumulator {
  double sumSquares[3] = {};
  double minAmps[3] = {INFINITY,INFINITY,INFINITY};
  double maxAmps[3] = {-INFINITY,-INFINITY,-INFINITY};
  uint32_t count = 0;
  uint32_t clipping = 0;
  uint32_t timeouts = 0;
  uint32_t invalid = 0;
  uint32_t drdySeen = 0;
  uint32_t lastReportMs = 0;
} stats;

bool readyCalibration() {
  if (!cfg::CALIBRATED) return false;
  for (int ch=0; ch<3; ++ch)
    if (!isfinite(cfg::SHUNT_OHM[ch]) || cfg::SHUNT_OHM[ch]<=0 ||
        !isfinite(cfg::AMP_GAIN[ch]) || cfg::AMP_GAIN[ch]<=0 ||
        !isfinite(cfg::ZERO_V[ch])) return false;
  return true;
}

// Nominal signed 24-bit ADS conversion result, gain=1. Output is differential volts.
float adcVolts(int32_t code) {
  return ((double)code / 8388608.0) * (cfg::ADC_VREF / cfg::ADC_PGA_GAIN);
}

void printReport() {
  Serial.printf("frames=%lu drdy=%lu timeouts=%lu clipped=%lu invalid=%lu ",
    (unsigned long)stats.count, (unsigned long)stats.drdySeen,
    (unsigned long)stats.timeouts, (unsigned long)stats.clipping,
    (unsigned long)stats.invalid);
  if (readyCalibration() && stats.count>0) {
    for (int ch=0; ch<3; ++ch) {
      Serial.printf("I%c_rms=%.6fA min=%.6f max=%.6f ",
        char('A'+ch), sqrt(stats.sumSquares[ch]/stats.count),
        stats.minAmps[ch], stats.maxAmps[ch]);
    }
  } else {
    Serial.print("I_RMS=NOT_CALIBRATED ");
  }
  Serial.printf("last_raw=[%ld,%ld,%ld,%ld] raw_ch3=%ld\n",
    (long)samples[0],(long)samples[1],(long)samples[2],(long)samples[3],(long)samples[3]);
  uint32_t t=millis();
  stats=Accumulator{};
  stats.lastReportMs=t;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Thunderbots ADS131M04 | bench-only V0.1");
  Serial.println("NO LIVE MD/24V until analog protection and calibration reviewed.");
  // SPI pins in PCB drawing. Driver begin() calls SPI.begin() again;
  // Arduino-ESP32 2.x retains the already initialized pin routing.
  pinMode(cfg::PIN_CS, OUTPUT);
  digitalWrite(cfg::PIN_CS,HIGH);
  pinMode(cfg::PIN_DRDY, INPUT_PULLUP);
  pinMode(cfg::PIN_RESET, OUTPUT);
  digitalWrite(cfg::PIN_RESET,HIGH);
  adcSPI.begin(cfg::PIN_SCLK,cfg::PIN_MISO,cfg::PIN_MOSI,cfg::PIN_CS);
  adc.begin(); // Driver generates ADC CLKIN; must confirm 8.192MHz on scope.
  delay(5);
  uint16_t id=adc.readReg(0x00);
  uint16_t clockReg=adc.readReg(0x03);
  uint16_t modeReg=adc.readReg(0x02);
  Serial.printf("ID=0x%04X MODE=0x%04X CLOCK=0x%04X\n",id,modeReg,clockReg);
  // Reject clearly broken SPI before claiming acquisition.
  if (id==0xFFFF || id==0x0000 || clockReg==0xFFFF) {
    Serial.println("ADC communication suspect: check power, CLKIN, MISO/MOSI, CS, DRDY");
    while(true) delay(1000);
  }
  // No OSR/gain setting performed: retain reset defaults pending register validation.
  if (!readyCalibration()) Serial.println("Current calibration unavailable; displaying raw ADC codes only.");
  stats.lastReportMs=millis();
}

void loop() {
  // Wait for the active-low DRDY; loop yields if no data rather than waiting indefinitely.
  static uint32_t lastReadyUs=micros();
  static bool prevHigh=true;
  bool high = digitalRead(cfg::PIN_DRDY)==HIGH;
  if (prevHigh && !high) {
    ++stats.drdySeen;
    adc.rawChannels(channels,4,samples);
    ++stats.count;
    for (int ch=0;ch<3;++ch) {
      if (samples[ch] >= 8388607 || samples[ch] <= -8388608) ++stats.clipping;
      if (readyCalibration()) {
        float amps=(adcVolts(samples[ch])-cfg::ZERO_V[ch]) /
                    (cfg::AMP_GAIN[ch]*cfg::SHUNT_OHM[ch]);
        if (isfinite(amps)) {
          stats.sumSquares[ch]+=(double)amps*amps;
          if (amps<stats.minAmps[ch]) stats.minAmps[ch]=amps;
          if (amps>stats.maxAmps[ch]) stats.maxAmps[ch]=amps;
        } else ++stats.invalid;
      }
    }
    lastReadyUs=micros();
  }
  prevHigh=high;
  if ((uint32_t)(micros()-lastReadyUs)>cfg::DRDY_TIMEOUT_US) {
    ++stats.timeouts;
    lastReadyUs=micros();
  }
  if ((uint32_t)(millis()-stats.lastReportMs)>=cfg::REPORT_MS) printReport();
}
