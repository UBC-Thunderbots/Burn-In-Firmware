[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = https://github.com/icl-rocketry/ADS131M04-Lib.git
; Driver uses ESP32 Arduino 2.x LEDC API, pin this core until ported.
platform_packages = platformio/framework-arduinoespressif32@~3.20017.0
