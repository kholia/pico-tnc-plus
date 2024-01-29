//

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include "config.h"

const char* host = "asia.aprs2.net";
const uint16_t port = 14580;

WiFiMulti multi;
WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, "time.google.com");

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/uart.h"
#include "pico/util/queue.h"
#include "hardware/watchdog.h"

#include "tnc.h"
#include "receive.h"
#include "send.h"
#include "ax25.h"
#include "beacon.h"

#include "AHTxx.h"
float ahtValue;                               // to store T/RH result
AHTxx aht20(AHTXX_ADDRESS_X38, AHT2x_SENSOR); // sensor address, sensor type

WiFiClient client;

char login_buffer[128];

void login_to_aprs_system()
{
  // Login to the APRS network
  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    delay(5000);
    return;
  }
  // wait for data to be available
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      delay(60000);
      return;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  Serial.println("receiving from remote server");
  // not testing 'client.connected()' since we do not need to send data here
  while (client.available()) {
    char ch = static_cast<char>(client.read());
    Serial.print(ch);
  }
  // This will send a string to the server
  Serial.println("sending data to server");
  if (client.connected()) {
    sprintf(login_buffer, "user %s pass %s vers %s\r\n", callsign, passcode, agent_version);
    Serial.println(login_buffer);
    client.println(login_buffer);
  }
  // wait for data to be available
  timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      delay(60000);
      return;
    }
  }
  // Read all the lines of the reply from server and print them to Serial
  Serial.println("receiving from remote server"); // # logresp VU3CER-7 verified, server T2FUKUOKA
  // not testing 'client.connected()' since we do not need to send data here
  while (client.available()) {
    char ch = static_cast<char>(client.read());
    Serial.print(ch);
  }
}

void setup() {
  Serial.begin(115200);
  /* if (watchdog_caused_reboot()) {
    printf("Watch Dog Timer Failure\n");
    } */

  // delay(5000);

  // Sensor stuff
  Wire.setSDA(12);
  Wire.setSCL(13);
  Wire.begin();
  while (aht20.begin() != true) {
    Serial.println(F("AHT2x not connected or fail to load calibration coefficient"));
    delay(5000);
  }
  Serial.println(F("AHT20 OK"));
  ahtValue = aht20.readTemperature();
  Serial.print(F("Temperature...: "));
  if (ahtValue != AHTXX_ERROR) {
    Serial.print(ahtValue);
    Serial.println(F(" +-0.3C"));
  }
  delay(2000); // required
  ahtValue = aht20.readHumidity();
  Serial.print(F("Humidity......: "));
  if (ahtValue != AHTXX_ERROR) {
    Serial.print(ahtValue);
    Serial.println(F(" +-2%"));
  }
  delay(2000);

  // WiFi stuff
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  multi.addAP(ssid, password);
  while (1) {
    if (multi.run(30000) != WL_CONNECTED) {
      Serial.println("Unable to connect to network, rebooting in 7 seconds...");
      delay(7000);
      // rp2040.reboot();
    } else {
      break;
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Login to the APRS network
  login_to_aprs_system();

  // NTP stuff
  timeClient.begin();
  timeClient.update();

  // Set watchdog, timeout 5000 ms
  watchdog_enable(5000, true);

  // Core init
  tnc_init();
  receive_init();
}

// TODO: Reduce dynamic memory usage
void wx_station_stuff()
{
  char buf[16];
  int temperatureInt;
  int humidityInt;
  char temperature_buf[4];
  char humidity_buf[3];

  Serial.println("WX station stuff!");
  String packet;

  // Get sensor values
  temperatureInt = (int)aht20.readTemperature();
  temperatureInt = (temperatureInt * 9 / 5.0) + 32;
  sprintf(temperature_buf, "%03d", temperatureInt);
  String temperature = temperature_buf;
  watchdog_update();
  delay(2000);
  watchdog_update();
  humidityInt = (int)aht20.readHumidity();
  sprintf(humidity_buf, "%02d", humidityInt);
  String humidity = humidity_buf;
  timeClient.update();
  watchdog_update();
  Serial.println(timeClient.getFormattedTime());
  watchdog_update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm  ts;
  ts = *localtime(&rawtime);
  strftime(buf, sizeof(buf), "%02d%02H%02Mz", &ts);
  Serial.printf("%s\n", buf);
  // https://aprs.fi/?c=raw&call=VU3ZAG*&limit=50&view=normal <-- debug using this url
  packet = String(callsign_wx) + ">APRS,TCPIP*,qAC,FIRST:@" + String(buf) + LAT + "/" + LON +
           "_.../...g...t" + temperature +
           "r...p...P...h" + humidity +
           "b.....h..L..." + COMMENT + "\n";
  Serial.println(packet);
  /* Note that a client is considered connected if the connection has been
     closed but there is still unread data. */
  while (client.available()) { // this loop is necessary to keep the TCP connection in a functional state!
    char ch = static_cast<char>(client.read());
    Serial.print(ch);
  }
  if (client.connected())
    client.print(packet);
  else {
    client.stop();
    Serial.println("CONNECTION LOST, Connecting again...");
    login_to_aprs_system();
    client.print(packet);
  }
}

uint64_t lastMillis;

uint32_t getTotalHeap(void) {
  extern char __StackLimit, __bss_end__;

  return &__StackLimit  - &__bss_end__;
}

uint32_t getFreeHeap(void) {
  struct mallinfo m = mallinfo();

  return getTotalHeap() - m.uordblks;
}

void loop() {
  // main loop
  while (1) {
    // Serial.println("XXXX");
    watchdog_update();

    if (millis() - lastMillis >= 2 * 60 * 1000UL) {
      // if (millis() - lastMillis >= 1 * 15 * 1000UL) {
      lastMillis = millis();  // get ready for the next iteration
      // Do WX station stuff here
      wx_station_stuff();
      Serial.println(getFreeHeap()); // memory leak detector
    }

    receive(); // All WiFi stuff needs to happen on Core 0
    // delay(100);
  }
}
