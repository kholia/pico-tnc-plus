#include <string.h>

#include "sauce.h"

#define _BV(bit) (1 << (bit))
#define typeof(x) __typeof__(x)
#define countof(a) sizeof(a)/sizeof(a[0])
#define MIN(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); ((typeof(_a))((_a < _b) ? _a : _b)); })
#define DECODE_CALL(buf, addr) for (unsigned i = 0; i < sizeof((addr))-CALL_OVERSPACE; i++) { char c = (*(buf)++ >> 1); (addr)[i] = (c == ' ') ? '\x0' : c; }
#define AX25_SET_REPEATED(msg, idx, val) do { if (val) { (msg)->rpt_flags |= _BV(idx); } else { (msg)->rpt_flags &= ~_BV(idx) ; } } while(0)

AX25Msg msg;

void ax25_decode(uint8_t *buf, int frame_len) {
  uint8_t *buf_start = buf;
  memset(&msg, 0, sizeof(AX25Msg));
  DECODE_CALL(buf, msg.dst.call);
  msg.dst.ssid = (*buf++ >> 1) & 0x0F;
  msg.dst.call[6] = 0;

  DECODE_CALL(buf, msg.src.call);
  msg.src.ssid = (*buf >> 1) & 0x0F;
  msg.src.call[6] = 0;

  for (msg.rpt_count = 0; !(*buf++ & 0x01) && (msg.rpt_count < countof(msg.rpt_list)); msg.rpt_count++) {
    DECODE_CALL(buf, msg.rpt_list[msg.rpt_count].call);
    msg.rpt_list[msg.rpt_count].ssid = (*buf >> 1) & 0x0F;
    AX25_SET_REPEATED(&msg, msg.rpt_count, (*buf & 0x80));
    msg.rpt_list[msg.rpt_count].call[6] = 0;
  }

  msg.ctrl = *buf++;
  if (msg.ctrl != AX25_CTRL_UI) {
    return;
  }

  msg.pid = *buf++;
  if (msg.pid != AX25_PID_NOLAYER3) {
    return;
  }

  msg.len = frame_len - 2 - (buf - buf_start);
  msg.info = buf;
}
