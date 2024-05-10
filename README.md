Please write simple description of your part so I can create the overview and constraints on system :D

https://docs.google.com/document/d/1az5H0Pi9jA3rXqkfopbbeTGD6ghsxjKuj-Lz2obhzMg/edit

# Temperature and Humidity Sensor
**Components:** DHT11 Temperature and Humidity Module, 10k resistor

**Description:** This module receives temperature and humidity data from the DHT11 sensor using the Arduino library DHT11. The temperature units are in Celsius and humidity is recorded by percentage.

# DC Motor & Fan Blade
**Components:** 3-6 V DC Motor, Fan Blade, L293D IC, Power Supply Module, 9V1A adapter

**Description:**
Used a power supply module with input voltage of 6.5-9v (DC) via 5.5mm x 2.1mm plug and output voltage of 3.3V/5V to prevent damage to the Arduino, since connecting the DC motor straight to it can cause damage. A L293D IC was used to control the DC motor. The fan blade is turned on when one input is HIGH and the other is LOW. The fan blade is off when both inputs are HIGH or both inputs are LOW.

**Spec Sheets:**
Mega 2560 Arduino Board Pinout - https://docs.arduino.cc/resources/pinouts/A000067-full-pinout.pdf
, L293D IC - https://www.ti.com/product/L293D

**LCD**
https://arduinogetstarted.com/tutorials/arduino-lcd
https://www.datasheethub.com/arduino-water-level-sensor/
