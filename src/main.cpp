#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <stdio.h>

#include "rtc_commands.h"

namespace {

RTC_DS3231 rtc;
RtcCommandParser commandParser;
uint32_t lastPrintTime = 0;

const uint32_t PRINT_INTERVAL_MS = 1000;

void printDateTime(const DateTime &dateTime) {
  char output[20];
  snprintf(output, sizeof(output), "%04u-%02u-%02u %02u:%02u:%02u",
           dateTime.year(), dateTime.month(), dateTime.day(), dateTime.hour(),
           dateTime.minute(), dateTime.second());
  Serial.println(output);
}

void printHelp() {
  Serial.println(F("Perintah yang tersedia:"));
  Serial.println(F("  set YYYY-MM-DD HH:MM:SS"));
  Serial.println(F("  now"));
  Serial.println(F("  help"));
  Serial.println(F("Gunakan aplikasi RTC Serial GUI untuk sinkronisasi otomatis."));
}

void executeCommand(const RtcCommand &command) {
  switch (command.type) {
    case RTC_COMMAND_SET: {
      const RtcDateTime &value = command.dateTime;
      rtc.adjust(DateTime(value.year, value.month, value.day, value.hour,
                          value.minute, value.second));
      Serial.print(F("OK: waktu RTC diatur ke "));
      printDateTime(rtc.now());
      break;
    }
    case RTC_COMMAND_NOW:
      Serial.print(F("RTC: "));
      printDateTime(rtc.now());
      break;
    case RTC_COMMAND_HELP:
      printHelp();
      break;
    case RTC_COMMAND_AUTO:
      Serial.println(F("ERROR: gunakan tombol sinkronisasi pada RTC Serial GUI."));
      break;
    case RTC_COMMAND_INVALID_DATE:
      Serial.println(F("ERROR: tanggal atau waktu tidak valid."));
      break;
    case RTC_COMMAND_INVALID_FORMAT:
      Serial.println(F("ERROR: format salah. Gunakan: set YYYY-MM-DD HH:MM:SS"));
      break;
    case RTC_COMMAND_UNKNOWN:
      Serial.println(F("ERROR: perintah tidak dikenal. Ketik 'help'."));
      break;
    case RTC_COMMAND_NONE:
      break;
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    RtcCommand command = {};
    const RtcFeedResult result =
        rtcCommandFeed(&commandParser, static_cast<char>(Serial.read()), &command);

    if (result == RTC_FEED_COMMAND_READY) {
      executeCommand(command);
    } else if (result == RTC_FEED_TOO_LONG) {
      Serial.println(F("ERROR: perintah terlalu panjang."));
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

#ifndef ESP8266
  while (!Serial) {
  }
#endif

  rtcCommandParserInit(&commandParser);

  if (!rtc.begin()) {
    Serial.println(F("RTC tidak terdeteksi! Periksa koneksi kabel."));
    Serial.flush();
    while (true) {
      delay(10);
    }
  }

  Serial.println(F("RTC DS3231 siap."));
  if (rtc.lostPower()) {
    Serial.println(F("PERINGATAN: RTC kehilangan daya. Lakukan sinkronisasi."));
  }
  printHelp();
}

void loop() {
  readSerialCommands();

  const uint32_t currentTime = millis();
  if (currentTime - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = currentTime;
    printDateTime(rtc.now());
  }
}
