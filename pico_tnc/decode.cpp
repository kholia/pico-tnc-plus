/*
  Copyright (c) 2021, Kazuhisa Yokota, JN1DFF
  All rights reserved.

  Copyright (c) 2024, Dhiru Kholia, VU3CER
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
  Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
  Neither the name of the <organization> nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Arduino.h>
#include <WiFi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"

#include "tnc.h"
#include "bell202.h"
#include "ax25.h"

extern WiFiClient client;

#define FCS_OK 0x0f47
// #define FCS_OK (0x0f47 ^ 0xffff)
#define MIN_LEN (7 * 2 + 1 + 1 + 2) // Address field *2, Control, PID, FCS
#define STR_LEN 64

// Dhiru
#include "sauce.h"
extern AX25Msg msg;
void ax25_decode(uint8_t *buf, int len);
bool KISS = false;
bool gotPacket = false;
char *srccall;
char *dstcall;
char *datapayload;
char packetchar[256]; /* buffer overflow is possible ;) */

static void output_packet(tnc_t *tp)
{
  int len = tp->data_cnt;
  uint8_t *data = tp->data;

  if (len < MIN_LEN) return;

  // FCS check
  if (ax25_fcs(0, data, len) != FCS_OK) return;

  // count received packet
  ++tp->pkt_cnt;

  // digipeat
  // if (param.digi) digipeat(tp);

  // Dhiru: IGate logic
  ax25_decode(data, len);
  srccall = msg.src.call;
  dstcall = msg.dst.call;
  char * datastr = (char*)msg.info;
  datastr[msg.len] = '\0';
  bool tcpippacket = false;
  // Fix for digirepeated TCPIP APRS.IS Packets
  if (datastr[0] == '}') {
    tcpippacket = true;
  }
  datapayload = datastr;
  char sssid[10];
  itoa(msg.src.ssid, sssid, 10);
  char dssid[10];
  itoa(msg.dst.ssid, dssid, 10);
  packetchar[0] = 0;
  strcat(packetchar, srccall);
  strcat(packetchar, "-");
  strcat(packetchar, sssid);
  strcat(packetchar, ">");
  strcat(packetchar, dstcall);
  // strcat (packetchar, "-");
  // strcat (packetchar, dssid);
  // https://github.com/ltiisidii/Igate-Digipeater-ESP8266/blob/main/src/APRS-ESP8266-IGATE.ino
  for (int i = 0; i < msg.rpt_count; i++) { // this loop is also executed!
    strcat(packetchar, ",");
    char *rptcall = msg.rpt_list[i].call;
    strcat(packetchar, rptcall);
    strcat(packetchar, "-");
    char rssid[10];
    itoa (msg.rpt_list[i].ssid, rssid, 10);
    strcat(packetchar, rssid);
  }
  strcat(packetchar, ":");
  int length_so_far = strlen(packetchar);
  memcpy(packetchar + length_so_far, datapayload, msg.len);
  memcpy(packetchar + length_so_far + msg.len, (char*)"\r\n", 2); // Write LF
  *(packetchar + length_so_far + msg.len + 2) = 0;
  // Don't IGATE TCPIP repeated packets
  if (!tcpippacket) {
    Serial.printf("IGate packet -> %s\n", packetchar);
    // Serial.printf("Lengths => %d %d\n", strlen(packetchar), length_so_far + msg.len);
    client.write(packetchar, length_so_far + msg.len + 2);
  }
}

#define AX25_FLAG 0x7e

static void decode_bit(tnc_t *tp, int bit)
{
  tp->flag <<= 1;
  tp->flag |= bit;

  switch (tp->state) {
    case FLAG:
      if (tp->flag == AX25_FLAG) { // found flag
        tp->state = DATA;
        tp->data_cnt = 0;
        tp->data_bit_cnt = 0;
        // fprintf(stderr, "found AX25_FLAG\n");
      }
      break;

    case DATA:
      if ((tp->flag & 0x3f) == 0x3f) { // AX.25 flag, end of packet, six continuous "1" bits
        output_packet(tp);
        tp->state = FLAG;
        break;
      }

      if ((tp->flag & 0x3f) == 0x3e) break; // delete bit stuffing bit

      tp->data_byte >>= 1;
      tp->data_byte |= bit << 7;
      tp->data_bit_cnt++;
      if (tp->data_bit_cnt >= 8) {
        if (tp->data_cnt < DATA_LEN) tp->data[tp->data_cnt++] = tp->data_byte;
        else {
          printf("packet too long > %d\n", tp->data_cnt);
          tp->state = FLAG;
          break;
        }
        tp->data_bit_cnt = 0;
      }
  }
}

#define PLL_STEP ((int)(((1ULL << 32) + SAMPLING_N/2) / SAMPLING_N))

// DireWolf PLL
static void decode2(tnc_t *tp, int val)
{
  int32_t prev_pll = tp->pll_counter >> 31; // compiler bug workaround

  tp->pll_counter += PLL_STEP;

  if ((tp->pll_counter >> 31) < prev_pll) { // overflow

    decode_bit(tp, val == tp->nrzi);    // decode NRZI
    tp->nrzi = val;
  }

  if (val != tp->pval) {

    // adjust PLL counter
    tp->pll_counter -= tp->pll_counter >> 2; // 0.75
    tp->pval = val;
  }
}

void demodulator(tnc_t *tp, int adc)
{
  int val;
  // int bit;

  // printf("%d,", adc);
  // dac_output_voltage(DAC_CHANNEL_1, adc >> 4);

#define AVERAGE_MUL 256
#define AVERAGE_N 64
#define AVERAGE_SHIFT 6
#define CDT_AVG_N 128
#define CDT_MUL 256
#define CDT_SHIFT 6

  // update average value
  tp->avg += (adc * AVERAGE_MUL - tp->avg) >> AVERAGE_SHIFT;
  val = adc - (tp->avg + AVERAGE_MUL / 2) / AVERAGE_MUL;

  // carrier detect
  tp->cdt_lvl += (val * val * CDT_MUL - tp->cdt_lvl) >> CDT_SHIFT;

#if 0
  static int count = 0;
  if ((++count & ((1 << 16) - 1)) == 0) {
    printf("(%u) decode: adc: %d, cdt_lvl: %d, avg: %d, port = %d\n", tnc_time(), adc, tp->cdt_lvl, tp->avg, tp->port);
  }
#endif

#define CDT_THR_LOW 1024
#define CDT_THR_HIGH (CDT_THR_LOW * 2) // low +6dB

  if (!tp->cdt && tp->cdt_lvl > CDT_THR_HIGH) { // CDT on

    gpio_put(tp->cdt_pin, 1);
    tp->cdt = true;
    //printf("(%u) decode: CDT on, adc: %d, cdt_lvl: %d, avg: %d, port = %d\n", tnc_time(), adc, tp->cdt_lvl, tp->avg, tp->port);
    //printf("(%u) decode: cdt on, port = %d\n", tnc_time(), tp->port);

  } else if (tp->cdt && tp->cdt_lvl < CDT_THR_LOW) { // CDT off

    gpio_put(tp->cdt_pin, 0);
    tp->cdt = false;
    //printf("(%u) decode: CDT off, adc: %d, cdt_lvl: %d, avg: %d, port = %d\n", tnc_time(), adc, tp->cdt_lvl, tp->avg, tp->port);
    //printf("(%u) decode: cdt off, port = %d\n", tnc_time(), tp->port);

  }

  if (!tp->cdt) return;

#if 0
  sum += adc;
  if (++count >= AVERAGE_N) {
    average = sum / AVERAGE_N;
    //ESP_LOGI(TAG, "average adc value = %d",  average);
    sum = 0;
    count = 0;
  }
#else
  //tp->average = (tp->average * (AVERAGE_N - 1) + adc + AVERAGE_N/2) / AVERAGE_N;
  /*
    if (count >= 13200) {
  	printf("average = %d\n", average);
  	count = 0;
    }
  */
#endif

#define LPF_N SAMPLING_N


  //val = bell202_decode((int)adc - average);
  //val = bell202_decode((int)adc - 2048);
  //val = bell202_decode(tp, adc); // delayed decode
#ifdef BELL202_SYNC
  val = bell202_decode2(tp, adc); // sync decode
#else
  val = bell202_decode(tp, adc); // delayed decode
#endif
  //lpf = (lpf * (LPF_N-1) + val) / LPF_N;

  //printf("demodulator(%d) = %d\n", adc, val);
#if 0
  static uint32_t count = 0;
  if (count < 132) {
    printf("%d, %d\n", adc, val);
  }
  if (++count >= 13200 * 10) {
    count = 0;
    printf("----------------\n");
  }
#endif

#define LPF_THRESHOLD (1 << 12)

  //printf("bit val = %d\n", val);

  if (val < -LPF_THRESHOLD) tp->bit = 1;
  else if (val >= LPF_THRESHOLD) tp->bit = 0;

  //dac_output_voltage(DAC_CHANNEL_2, bit * 128);
  //printf("%d\n", bit);
#ifdef DECODE_PLL
  decode2(tp, tp->bit);   // Direwolf PLL
#else
  decode(tp, tp->bit);    // bit length
#endif
}
