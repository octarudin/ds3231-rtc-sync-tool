#include "rtc_commands.h"

#include <string.h>

static uint8_t isLeapYear(uint16_t year) {
  return (uint8_t)((year % 4U == 0U && year % 100U != 0U) ||
                   year % 400U == 0U);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  if (month < 1U || month > 12U) {
    return 0U;
  }
  if (month == 2U && isLeapYear(year)) {
    return 29U;
  }
  return days[month - 1U];
}

static uint8_t readTwoDigits(const char *text, uint8_t *value) {
  if (text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9') {
    return 0U;
  }
  *value = (uint8_t)((text[0] - '0') * 10 + (text[1] - '0'));
  return 1U;
}

static uint8_t readFourDigits(const char *text, uint16_t *value) {
  uint8_t index;
  uint16_t result = 0U;
  for (index = 0U; index < 4U; index++) {
    if (text[index] < '0' || text[index] > '9') {
      return 0U;
    }
    result = (uint16_t)(result * 10U + (uint16_t)(text[index] - '0'));
  }
  *value = result;
  return 1U;
}

static RtcCommandType parseSetCommand(const char *text, RtcDateTime *dateTime) {
  if (strlen(text) != 23U || text[8] != '-' || text[11] != '-' ||
      text[14] != ' ' || text[17] != ':' || text[20] != ':') {
    return RTC_COMMAND_INVALID_FORMAT;
  }

  if (!readFourDigits(text + 4, &dateTime->year) ||
      !readTwoDigits(text + 9, &dateTime->month) ||
      !readTwoDigits(text + 12, &dateTime->day) ||
      !readTwoDigits(text + 15, &dateTime->hour) ||
      !readTwoDigits(text + 18, &dateTime->minute) ||
      !readTwoDigits(text + 21, &dateTime->second)) {
    return RTC_COMMAND_INVALID_FORMAT;
  }

  if (dateTime->year < 2000U || dateTime->year > 2099U ||
      dateTime->month < 1U || dateTime->month > 12U ||
      dateTime->day < 1U ||
      dateTime->day > daysInMonth(dateTime->year, dateTime->month) ||
      dateTime->hour > 23U || dateTime->minute > 59U ||
      dateTime->second > 59U) {
    return RTC_COMMAND_INVALID_DATE;
  }

  return RTC_COMMAND_SET;
}

static void parseCommand(char *text, RtcCommand *command) {
  char *start = text;
  char *end;

  while (*start == ' ' || *start == '\t') {
    start++;
  }
  end = start + strlen(start);
  while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
    *--end = '\0';
  }

  memset(command, 0, sizeof(*command));
  if (strncmp(start, "set", 3U) == 0) {
    command->type = parseSetCommand(start, &command->dateTime);
  } else if (strcmp(start, "now") == 0) {
    command->type = RTC_COMMAND_NOW;
  } else if (strcmp(start, "help") == 0 || strcmp(start, "?") == 0) {
    command->type = RTC_COMMAND_HELP;
  } else if (strcmp(start, "auto") == 0) {
    command->type = RTC_COMMAND_AUTO;
  } else if (*start != '\0') {
    command->type = RTC_COMMAND_UNKNOWN;
  }
}

void rtcCommandParserInit(RtcCommandParser *parser) {
  memset(parser, 0, sizeof(*parser));
}

RtcFeedResult rtcCommandFeed(RtcCommandParser *parser, char received,
                             RtcCommand *command) {
  if (received == '\r' || received == '\n') {
    if (parser->discarding) {
      parser->discarding = 0U;
      parser->length = 0U;
      return RTC_FEED_NONE;
    }
    if (parser->length == 0U) {
      return RTC_FEED_NONE;
    }

    parser->buffer[parser->length] = '\0';
    parseCommand(parser->buffer, command);
    parser->length = 0U;
    return RTC_FEED_COMMAND_READY;
  }

  if (parser->discarding) {
    return RTC_FEED_NONE;
  }
  if (parser->length >= RTC_COMMAND_BUFFER_SIZE - 1U) {
    parser->length = 0U;
    parser->discarding = 1U;
    return RTC_FEED_TOO_LONG;
  }

  parser->buffer[parser->length++] = received;
  return RTC_FEED_NONE;
}
