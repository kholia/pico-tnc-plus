// WiFi details
const char *ssid = "ssid";
const char *password = "password";

// Operator details
const char *callsign = "VU3CER-7";
const char *callsign_wx = "VU3CER-WX";
const char *passcode = "value";
const char *agent_version = "TestClient 0.0.1";
const String COMMENT = "PicoW RF IGate - https://github.com/kholia";

// Station details - Get your values using the https://aprs.fi/ site
const String LAT = "1829.50N";
const String LON = "07357.50E";

/*
The APRS protocol uses Latitude and Longitude like:

DDMM.hhN/DDDMM.hhW

where DDMM.hhN is Latitude
DD are 2 digits for degrees
MM are 2 digits for minutes
hh are 2 digits for hundreds of minutes
N is 1 digit for North or South(N, S)

where DDDMM.hhW is Longitude
DDD are 3 digits for degrees
MM are 2 digits for minutes
hh are 2 digits for hundreds of minutes
W is 1 digit for East or West(E, W)
*/

// http://www.aprs.org/doc/APRS101.PDF
// http://www.aprs.net/vm/DOS/PROTOCOL.HTM
// http://www.aprs.org/doc/APRS101.PDF
// https://aprs.fi/?c=raw&call=VU3CER*&limit=50&view=normal <-- super useful for debugging
// https://github.com/glidernet/ogn-aprs-protocol/blob/master/APRS-q-msg.txt

/*

Day/Hours/Minutes (DHM) format is a fixed 7-character field, consisting of
a 6-digit day/time group followed by a single time indicator character (z or
/). The day/time group consists of a two-digit day-of-the-month (01–31) and
a four-digit time in hours and minutes.
Times can be expressed in zulu (UTC/GMT) or local time. For example:

092345z -> is 2345 hours zulu time on the 9th day of the month.
092345/ -> is 2345 hours local time on the 9th day of the month.

*/
