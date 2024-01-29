#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define AX25_MIN_FRAME_LEN 18
#ifndef CUSTOM_FRAME_SIZE
#define AX25_MAX_FRAME_LEN 330
#else
#define AX25_MAX_FRAME_LEN CUSTOM_FRAME_SIZE
#endif

#define AX25_CRC_CORRECT  0xF0B8

#define AX25_CTRL_UI      0x03
#define AX25_PID_NOLAYER3 0xF0

#define AX25_CALL(str, id) {.call = (str), .ssid = (id) }
#define AX25_MAX_RPT 8

#define CALL_OVERSPACE 1

typedef struct AX25Call {
  char call[6 + CALL_OVERSPACE];
  uint8_t ssid;
} AX25Call;

typedef struct AX25Msg {
  AX25Call src;
  AX25Call dst;
  AX25Call rpt_list[AX25_MAX_RPT];
  uint8_t  rpt_count;
  uint8_t  rpt_flags;
  uint16_t ctrl;
  uint8_t  pid;
  const uint8_t *info;
  size_t len;
} AX25Msg;
