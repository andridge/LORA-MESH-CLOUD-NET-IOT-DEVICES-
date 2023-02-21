#include <RHRouter.h>
#include <RHReliableDatagram.h>
#include <RH_DHT22.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

// LoRa configuration
#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 255
#define ROUTER_ADDRESS 2
#define FREQUENCY 915.0
#define SPREADING_FACTOR 7
#define TX_POWER 20

// DHT22 sensor configuration
#define DHT22_PIN 2

// RTC configuration
RTC_DS3231 rtc;

// LoRa objects
RHRouter router(ROUTER_ADDRESS, SPREADING_FACTOR);
RHReliableDatagram manager(router, CLIENT_ADDRESS);

// Data buffer
uint8_t data[5];

// Sleep time
const uint32_t SLEEP_TIME = 300000; // 5 minutes

void setup() {
  Serial.begin(9600);
  if (!manager.init()) {
    Serial.println("LoRa initialization failed");
    while (1);
  }
  manager.setThisAddress(CLIENT_ADDRESS);
  router.setFrequency(FREQUENCY);
  router.setTxPower(TX_POWER);
  router.testNetwork();
  rtc.begin();
  // set initial alarm
  rtc.setAlarm1(DateTime(rtc.now() + TimeSpan(0, 0, 30, 0)));
  rtc.enableAlarm(rtc.MATCH_1);
  attachInterrupt(digitalPinToInterrupt(2), wakeUp, FALLING);
}

void loop() {
  if (digitalRead(DHT22_PIN) == HIGH) {
    Serial.println("DHT22 sensor communication failure");
    return;
  }
  float temperature = dht22.temperature();
  float humidity = dht22.humidity();
  data[0] = (uint8_t)(temperature);
  data[1] = (uint8_t)(temperature * 100) % 100;
  data[2] = (uint8_t)(humidity);
  data[3] = (uint8_t)(humidity * 100) % 100;
  data[4] = (uint8_t)(rtc.now().unixtime() >> 24);
  if (manager.sendtoWait(data, sizeof(data), SERVER_ADDRESS)) {
    Serial.println("Data sent successfully");
  } else {
    Serial.println("Data sending failed");
    if (!router.routingTableHasRouteTo(SERVER_ADDRESS)) {
      // Self-configure client address
      uint8_t newAddress = router.getNextAvailableClientAddress();
      if (newAddress != RH_ROUTER_BROADCAST_ADDRESS) {
        manager.setThisAddress(newAddress);
        CLIENT_ADDRESS = newAddress;
        Serial.print("Self-configured client address: ");
        Serial.println(CLIENT_ADDRESS);
      }
    }
  }
    // listen for incoming data from other nodes and forward to server node
  if (manager.available()) {
    // Wait for a message addressed to this node
    uint8_t from, len;
    if (manager.recvfromAck(data, &len, &from)) {
      Serial.print("Received from node: ");
      Serial.println(from);
      if (from != CLIENT_ADDRESS && from != SERVER_ADDRESS) {
        // Forward the message to the server node
        if (manager.sendtoWait(data, len, SERVER_ADDRESS)) {
          Serial.println("Data forwarded successfully");
        } else {
          Serial.println("Data forwarding failed");
        }
      }
    }
  }
  router.printRoutingTable(Serial);
  // set next alarm
  rtc.setAlarm1(DateTime(rtc.now() + TimeSpan(0, 0, 30, 0)));
  rtc.enableAlarm(rtc.MATCH_1);


  // enter sleep mode
  LowPower.sleep();
}

void wakeUp() {
  // clear alarm flag
  rtc.clearAlarm(rtc.MATCH_1);
}
