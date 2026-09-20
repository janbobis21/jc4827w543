JC4827W543 4.3" RGB Display + GT911 Touch (ESP32‑S3)

Overview
This repository provides a beginner‑friendly reference for integrating the JC4827W543 4.3" 480×272 RGB display with NV3041A display and GT911 capacitive touch on an ESP32‑S3.

For the NV3041A display, the following pins are connected to the ESP32-S3 through the following SPI Pins:
GPIO45 - CS 
GPIO47 - SCK
GPIO21 - D0
GPIO48 - D1
GPIO40 - D2
GPIO39 - D3

For the GT911, the followig pins are connected through the following I2C Pins:
GPIO8 - I2C_SDA
GPIO4 - I2C_SCL
GPIO3 - TOUCH_INT
GPIO38 - TOUCH_RST

For the sample codes included, make sure that the following libraries are installed:
- GFX Library for Arduino (by Moon on our Nation)
- TAMC_GT911 (by TAMC)
