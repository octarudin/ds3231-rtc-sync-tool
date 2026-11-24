#ifndef RTC_COMMANDS_H
#define RTC_COMMANDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_COMMAND_BUFFER_SIZE 40

typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} RtcDateTime;

typedef enum {
  RTC_COMMAND_NONE,
  RTC_COMMAND_SET,
  RTC_COMMAND_NOW,
  RTC_COMMAND_HELP,
  RTC_COMMAND_AUTO,
  RTC_COMMAND_INVALID_DATE,
  RTC_COMMAND_INVALID_FORMAT,
  RTC_COMMAND_UNKNOWN
} RtcCommandType;

typedef struct {
  RtcCommandType type;
  RtcDateTime dateTime;
} RtcCommand;

typedef struct {
  char buffer[RTC_COMMAND_BUFFER_SIZE];
  size_t length;
  uint8_t discarding;
} RtcCommandParser;

typedef enum {
  RTC_FEED_NONE,
  RTC_FEED_COMMAND_READY,
  RTC_FEED_TOO_LONG
} RtcFeedResult;

void rtcCommandParserInit(RtcCommandParser *parser);
RtcFeedResult rtcCommandFeed(RtcCommandParser *parser, char received,
                             RtcCommand *command);

#ifdef __cplusplus
}
#endif

#endif
