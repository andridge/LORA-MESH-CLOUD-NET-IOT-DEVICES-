#include <Wire.h>
#include <DS3232RTC.h>
#include <LowPower.h>
#include <DHT.h>
#include <SPI.h>
#include <LoRa.h>

#define RTC_ADDRESS 0x68
#define DHT_PIN 2
#define SENDER_ADDRESS 1
#define RECEIVER_ADDRESS 2
#define WAKEUP_INTERVAL 86400 / 5 // Wake up 5 times a day (every 5 * 86400 seconds)

DHT dht(DHT_PIN, DHT22);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  LoRa.setPins(10, 9, 2); // LoRa module CS, reset, and IRQ pins
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa initialization failed.");
    while (1);
  }

  setSyncProvider(RTC.get); // Synchronize time with DS3232 clock
  if (timeStatus() != timeSet) {
    Serial.println("Unable to sync with the RTC.");
  }

  dht.begin();
}

void loop() {
  float temperature, humidity;

  // Calculate the next wake-up time
  time_t now = time(nullptr);
  time_t wakeUpTime = ((now / WAKEUP_INTERVAL) + 1) * WAKEUP_INTERVAL;

  // Set the DS3232 alarm to the wake-up time
  Wire.beginTransmission(RTC_ADDRESS);
  Wire.write(0x07); // First alarm (A1) time and date register
  Wire.write(0x80); // Set the alarm to activate only when the seconds match
  Wire.write(wakeUpTime % 60); // Set the seconds alarm
  Wire.write(0x00); // Set the minutes alarm
  Wire.write(0x00); // Set the hours alarm
  Wire.write(0x00); // Set the day alarm (ignored)
  Wire.write(0x00); // Set the date alarm (ignored)
  Wire.write(0x00); // Set the month alarm (ignored)
  Wire.endTransmission();

  // Sleep until the next wake-up time
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  // Read the sensor data
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Send the sensor data through LoRa
  String dataString = String(SENDER_ADDRESS) + "," + String(temperature) + "," + String(humidity);
  LoRa.beginPacket();
  LoRa.write(RECEIVER_ADDRESS);
  LoRa.print(dataString);
  LoRa.endPacket();

  // Print the sensor data to the serial monitor
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.print(" °C\t");
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  // Delay for a moment to allow the data to be sent
  delay(1000);
}
