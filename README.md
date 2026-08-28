# ServoControllerESP8266ESP32
This code can be used for PCA9685 together with ESP32 and ESP8266 to control via servo.local.


Here is the configuration

PCA9685 Pin,ESP32 Pin,ESP8266 (NodeMCU / Wemos) Pin,Description
VCC,3.3V,3.3V,Logic power for the PCA9685 chip
GND,GND,GND,Common ground
SDA,GPIO 21,D2 (GPIO 4),I2C Data Line
SCL,GPIO 22,D1 (GPIO 5),I2C Clock Line
V+,5V / External,5V / External,Power supply for the Servos (Do not power 16 servos from the ESP's 5V pin)
