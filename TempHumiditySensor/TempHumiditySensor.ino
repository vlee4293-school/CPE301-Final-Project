
#include <DHT11.h>
#include <LiquidCrystal.h>

#define DHTPIN 8

DHT11 dht11(DHTPIN);
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

void setup() {
  lcd.begin(16, 2);
}

void loop() {
  int temperature = 0;
  int humidity = 0;

  int result = dht11.readTemperatureHumidity(temperature, humidity);

  if (result == 0) {
    lcd.setCursor(0, 0);
    lcd.print(String("Temp = ") + String(temperature) + String(" C"));
    lcd.setCursor(0, 1);
    lcd.print(String("RH   = ") + String(humidity) + String(" %"));
  }
}
