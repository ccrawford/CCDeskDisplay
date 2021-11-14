/*
 * Nexion Control Panel.
 * Chris Crawford, September 2021
 * 
 * This project uses and ESP32 dev board connected to an Nextion NX4832K035_011 (320x480 3.5" enhanced display)
 * Connect the Nextion to the ESP32 second serial port (RX2 and TX2). That's it.
 * 
 * Yahoo API info: https://www.reddit.com/r/sheets/comments/ji52uk/yahoo_finance_api_url/
 * https://www.reddit.com/r/sheets/wiki/apis/finance#wiki_multiple_lookup
 * https://stackoverflow.com/questions/44030983/yahoo-finance-url-not-working
 * 
 * Test stock API calls:
 * URLS: https://query1.finance.yahoo.com/v8/finance/chart/ACN?interval=2m
 * https://query1.finance.yahoo.com/v7/finance/quote?lang=en-US&region=US&corsDomain=finance.yahoo.com&symbols=ACN,^GSPC,^IXIC
 * https://query1.finance.yahoo.com/v10/finance/quoteSummary/ACN?modules=price
 * 
 * Libraries:
 * https://github.com/Seithan/EasyNextionLibrary
 * https://www.flaticon.com/packs/music-and-video-app-4
 * https://github.com/knolleary/pubsubclient
 * 
 * Supporting technology:
 * https://github.com/TroyFernandes/hass-mqtt-mediaplayer
 * 
 * Libraries:
 * https://github.com/107-systems/107-Arduino-Debug Debug Macros
 * https://github.com/Seithan/EasyNextionLibrary Useful here. 
 * https://github.com/eduardomarcos/arduino-esp32-restclient  BUT YOU HAVE TO MODIFY TO USE "CLIENT" NOT "CLIENT SECURE" in library. Should add that.
 *  So, in RestClient.h in the library, comment out WiFiClientSecure line and make the client_s variable type WiFiClient. 
 * https://arduinojson.org/ Massive, helpful library.
 * https://github.com/knolleary/pubsubclient ("Arduino Client for MQTT" by Nick O'Leary)
 */


#define DBG_ENABLE_VERBOSE
#define DBG_ENABLE_WARNING
#define DBG_ENABLE_INFO
#define DBG_ENABLE_DEBUG
#define ESP32_RESTCLIENT_DEBUG

#include <ArduinoDebug.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <EasyNextionLibrary.h>
#include <RestClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "YahooFin.h"
#include "CCSecrets.h" //Tokens, passwords, etc.

DEBUG_INSTANCE(160, Serial);


// restClient is used to make API requests via the HomeAssistant server to control the Sonos.
RestClient restClient = RestClient(haServer, 8123);

EasyNex myNex(Serial2);

WiFiClient (espClient);
PubSubClient client(espClient);

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_USE_DOUBLE 1

const char* ssid = STASSID;
const char* password = STAPSK;

char playerState[20];
int trackDuration;
int trackPosition;


// MQTT callback for media message
void callback(char* topic, byte* payload, unsigned int length) {
  DBG_DEBUG("In callback");
  
  if(myNex.currentPageId != 3) {
    DBG_INFO("Not on page3, skipping updating media player info.");
    return;
  }

//  char quote_msg[26];
//  sprintf(quote_msg, "$%.2f($%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
 
//  myNex.writeStr(field + ".txt", quote_msg);
  
  DBG_INFO("Message arrived [%s]");

  if(!strcmp(topic, "homeassistant/media_player/volume")) {
    char bufVol[6];
    strncpy(bufVol, (char *)payload, length);
    bufVol[length] = 0;
    int vol = atof(bufVol)*100;
    DBG_INFO("Volume: %d", vol);
    myNex.writeNum("j1.val", vol);
  }

  if(!strcmp(topic, "homeassistant/media_player/track")) {
    if (length>100) length=100;
    char track[101]; 
    strncpy(track, (char *)payload, length);
    track[length] = 0;
    DBG_INFO("track: %s", track);
    myNex.writeStr("t0.txt", track);
  }
  if(!strcmp(topic, "homeassistant/media_player/state")) {
    char bufState[20];
    strncpy(bufState, (char *)payload, length);
    bufState[length]=0;
    strcpy(playerState, bufState);
    DBG_INFO("State: %s", playerState);
    if(!strcmp("playing",bufState)) {
      myNex.writeNum("tm0.en",1);
      myNex.writeStr("vis p2,0");
      myNex.writeStr("vis p7,1");
    } else {
      myNex.writeNum("tm0.en",0);
      myNex.writeStr("vis p2,1");    
      myNex.writeStr("vis p7,0");
    }
  }
  if(!strcmp(topic, "homeassistant/media_player/artist")) {
    if (length>100) length=100;
    char artist[101];
    strncpy(artist, (char *)payload, length);
    artist[length] = 0;
    DBG_INFO("Artist: %s", artist);
    myNex.writeStr("t1.txt", artist);
  }
  if(!strcmp(topic, "homeassistant/media_player/duration")) {
    char duration[6];
    strncpy(duration, (char *)payload, length);
    duration[length] = 0;
    trackDuration = atoi(duration);

    // We'll use that timer to update the progress. Since progress is always 0-100, we need to set the timer
    // to tick every 1% of the track. Timer is in ms. Duration in seconds. Progress bar in %. So, multiply by 1000/100=10.
    myNex.writeNum("tm0.tim", trackDuration*10); 
    DBG_INFO("Duration: %d",trackDuration);
    
  }
  if(!strcmp(topic, "homeassistant/media_player/position")) {
    char bufPosition[6];
    strncpy(bufPosition, (char *)payload, length);
    bufPosition[length] = 0;
    trackPosition = atoi(bufPosition);
    DBG_INFO("Position: %d", trackPosition);
  }

   if(!strcmp(topic, "homeassistant/media_player/position_last_update")) {
    int yr,mo,da,hr,mn,se;
    struct tm timeinfo;
    
    char bufTime[36];
    strncpy(bufTime, (char *)payload, length);
    bufTime[length] = 0;

    sscanf(bufTime, "%d-%d-%d %d:%d:%d\+*",&yr,&mo,&da,&hr,&mn,&se);
    timeinfo.tm_sec = se;
    timeinfo.tm_min = mn;
    timeinfo.tm_hour = hr;
    timeinfo.tm_mday = da;
    timeinfo.tm_mon = mo-1;
    timeinfo.tm_year = yr-1900;

    time_t now;
    time(&now);
    int diffTime = difftime(mktime(gmtime(&now)), mktime(&timeinfo));
    DBG_INFO("DiffTime: %d", diffTime);

    // If this is a new time update, then reset the time. 
    // TODO: deal with a mid-track update...messy.
    // Should also check position here...
    if(diffTime<=1) {
      int curOffset = (int)((trackPosition*100)/trackDuration); // Calculate what pct of track has been played.
      myNex.writeNum("j0.val", curOffset);
    }
  
  }
 
/*  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  */
}


void reconnect() {
  // Loop until we're reconnected to the MQTT broker.
  while (!client.connected()) {
    DBG_INFO("Attempting MQTT connection...");
    
    if (client.connect("DesktopBuddy","hass.mqtt","trixie*1",0,0,0,0,0)) {
      DBG_INFO("connected");
      client.subscribe("homeassistant/media_player/#");
    } else {
      DBG_ERROR("failed to connect to MQTT, rc=%d Try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// Update the RTC in the Nextion. Actual time dispaly handled over there. 
void setNexionTime()
{
  time_t rawtime;
  struct tm * timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  
  char tmStrBuf[10];
  sprintf(tmStrBuf,"rtc%d=%d",5,timeinfo->tm_sec);
  myNex.writeStr(tmStrBuf);
  sprintf(tmStrBuf,"rtc%d=%d",4,timeinfo->tm_min);
  myNex.writeStr(tmStrBuf);
  sprintf(tmStrBuf,"rtc%d=%d",3,timeinfo->tm_hour);
  myNex.writeStr(tmStrBuf);
  sprintf(tmStrBuf,"rtc%d=%d",1,timeinfo->tm_mon+1);
  myNex.writeStr(tmStrBuf);
  sprintf(tmStrBuf,"rtc%d=%d",2,timeinfo->tm_mday);
  myNex.writeStr(tmStrBuf);
  sprintf(tmStrBuf,"rtc%d=%d",0,timeinfo->tm_year+1900);
  myNex.writeStr(tmStrBuf);
  
}

void getQuote(char* symbol, String field)
{
  YahooFin yf = YahooFin(symbol);  
  yf.getQuote();

  char quote_msg[30];
  sprintf(quote_msg, "$%.2f($%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
 
  myNex.writeStr(field + ".txt", quote_msg);
  
  if (yf.regularMarketChange < 0) myNex.writeNum(field + ".pco", 63488);
  else myNex.writeNum(field + ".pco", 34784);
  
}

void updateGraph(char * symbol) {
  if(myNex.currentPageId != 2) {
    DBG_INFO("Not on page2, skipping graph stuff.");
    return;
  }

  // Update the detailed quote on page.
  char quote_msg[30];
  YahooFin yf = YahooFin(symbol);
  yf.getQuote();

  sprintf(quote_msg, "%.2f(%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
  if (yf.regularMarketChange < 0) myNex.writeNum("t1.pco", 63488);
  else myNex.writeNum("t1.pco", 34784);
  
  myNex.writeStr("t1.txt", quote_msg);

  // Update the chart
  yf.getChart();

  if (yf.minuteDataPoints == 0) return;

  // Clear the graph...we can think about adding on to it later. 
  myNex.writeStr("cle 2,0");
  myNex.writeStr("cle 2,1");

  // Change the line color based on up/down
  if(yf.regularMarketChange<0) myNex.writeNum("s0.pco0", 63488);
  else myNex.writeNum("s0.pco0", 34784);

  // Figure out scale  
  long scaleLow = floor(min(yf.regularMarketPreviousClose, yf.regularMarketDayLow)) * 100;
  long scaleHigh = ceil(max(yf.regularMarketPreviousClose, yf.regularMarketDayHigh)) * 100;

  char pc[18];  // pc is the previous close amount for the line.
  sprintf(pc, "add 2,1,%d", map((long)(yf.regularMarketPreviousClose*100), scaleLow,scaleHigh,0,255));
    
  int high = 0;
  int highI = 0;
  int low = 999;
  int lowI = 0;
  int j = 0;

  for(int i = 0; i < yf.minuteDataPoints; i++)
  {
    
    if(yf.minuteQuotes[i] > 0) {

      long mappedVal = map((long)(yf.minuteQuotes[i]*100), scaleLow,scaleHigh,0,255);

      if(mappedVal >= high) {
        high = mappedVal;
        highI = j;
      }

      if(mappedVal <= low) {
        low = mappedVal;
        lowI = j;
      }

      myNex.writeStr("add 2,0," + String(mappedVal));
      myNex.writeStr(pc);
      j++;
      
      //Stretch the graph a bit
      if(i % 3) {
        myNex.writeStr("add 2,0," + String(mappedVal));
        myNex.writeStr(pc);
        j++;
      }
    }
  }

  // Update the min/max/last overlay

  delay(40); //Delay allows the transparent text to work.
  
  char controlDesc[50];
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", min(245,highI), 255-high-4, yf.regularMarketDayHigh);
  myNex.writeStr(controlDesc);
  
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", min(245,lowI), 255-low+26, yf.regularMarketDayLow);
  myNex.writeStr(controlDesc);
    
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", min(245,j), 255-map((long)(yf.regularMarketPreviousClose*100), scaleLow,scaleHigh,0,255), yf.regularMarketPreviousClose);
  myNex.writeStr(controlDesc);

  return;

}

// Select the current source for Sonos. Has to be in the Sonos favorites.
void selectSource(char* channelName) {
  char postParameter[80];
  sprintf(postParameter, "{\"entity_id\":\"media_player.sonos_5\", \"source\":\"%s\"}", channelName);
  
  restClient.setHeader(HA_TOKEN);
  restClient.post("/api/services/media_player/select_source", postParameter);
  
}

void mediaControl(char* command) {

  // See: https://www.home-assistant.io/integrations/media_player
  
  char service[100];
  sprintf(service, "/api/services/media_player/%s", command);
  
  restClient.setHeader(HA_TOKEN);
  restClient.post(service, "{\"entity_id\":\"media_player.sonos_5\"}");
  
}

void setup() {
  Serial.begin(115200);
  // Don't "Debug" this out. I want this to print regardless. Not time sensitive and very useful when you find an old ESP lying around.
  Serial.println(F("YahooNexionStockTicker Sep 2021"));

  myNex.begin(115200);
  
  DBG_INFO("Connecting to %s", ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_INFO(".");
  }

  DBG_INFO("IP address: %d:%d:%d:%d", WiFi.localIP()[0],WiFi.localIP()[1],WiFi.localIP()[2],WiFi.localIP()[3]);

  // Set time via NTP, as required for x.509 validation
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);  // Chicago time zone via: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  tzset();

  DBG_INFO("Waiting for NTP time sync: ");
  time_t now = time(NULL);
  while (now < 8 * 3600 * 2) {
    delay(500);
    DBG_INFO(".");
    now = time(NULL);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  setNexionTime();    

  // Setup MQTT
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  myNex.writeStr("page 0");

//  myNex.lastCurrentPageId = 1;
//  myNex.currentPageId = myNex.readNumber("dp");

  // Update quote info
  // updateQuotes();
  updateGraph("ACN");

  setNextionBrightness(100);
  
  DBG_DEBUG("=================SETUP DONE=================");
}

void setNextionBrightness(int brightness)
{
  static int curBrightness = -1;
          
  if (brightness < 0 || brightness > 100) return;

  if (brightness != curBrightness) {
    char msg[9];
    sprintf(msg, "dim=%d", brightness);
    myNex.writeStr(msg);
    DBG_INFO(msg);
    curBrightness = brightness;
  }
}

void updateQuotes()
{
  DBG_INFO("Cur page: %d", myNex.currentPageId);
  if(myNex.currentPageId != 0) {
    DBG_INFO("Not on page0, skipping.");
    return;
  }
  myNex.writeStr("t7.txt","updating.");
  getQuote("ACN", "t1");
  myNex.writeStr("t7.txt","updating .");
  getQuote("^GSPC", "t4");        
  myNex.writeStr("t7.txt","updating  .");
  getQuote("^IXIC", "t5");
  myNex.writeStr("t7.txt","");
}

void trigger0() {
  mediaControl("media_play_pause");
}
// Turn office light on/off.
void trigger1() {
  restClient.setHeader(HA_TOKEN);
  int statusCode = restClient.post("/api/services/light/toggle",  "{\"entity_id\":\"light.office_light\"}");
  
}
void trigger2() {
  selectSource("WXRT Over the Air");
}
void trigger3() {
  DBG_INFO("Vol down...");
  mediaControl("volume_down");
}
void trigger4() {
  DBG_INFO("Vol up...");
  mediaControl("volume_up");
}

void trigger6() {
  selectSource("Discover Weekly");
}
void trigger7() {
  selectSource("Daily Mix 1");
}
void trigger8() {
  selectSource("Daily Mix 2");
}
void trigger9() {
  selectSource("Daily Mix 3");
}
void trigger10() {
  selectSource("Daily Mix 4");
}
void trigger11() {
  selectSource("Daily Mix 5");
}
void trigger12() {
  selectSource("Daily Mix 6");
}
void trigger13() {
  mediaControl("media_next_track");
}
void trigger14() {
  selectSource("media_previous_track");
}
void trigger16() {
  updateQuotes();
}
void trigger17() {
    updateGraph("ACN");
}
void trigger18() {
  DBG_DEBUG("Update the player info, if possible.");
}

unsigned long lastRefresh = 0;

void loop() {
  
  struct tm timeinfo;
  static int timeSetDay = -1;
  myNex.NextionListen();

  // Do MQTT checks
  if (!client.connected()) {
    DBG_INFO("reconnecting...");
    reconnect();
  }
  client.loop();

  // Refresh every minute when market is open.
  // Also do basic housekeeping every minute.
 
  if((millis() - lastRefresh) > 60000) {
    
    lastRefresh = millis();
    if (!getLocalTime(&timeinfo)) DBG_ERROR("Couldn't get local time");
    DBG_VERBOSE("Current time: %d:%2d:%2d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  //I'm using Chicago time here, but markets operate on Eastern time. 
  // Probably a bad idea (GMT wouldn't be any better), but too much trouble to shift to Eastern.
  // Market open 8:30am to 4pm, M-F. Don't stress about market holidays. 
    
    int secondsSinceMidnight = timeinfo.tm_hour*3600 + timeinfo.tm_min*60 + timeinfo.tm_sec;
    if((timeinfo.tm_wday >= 1 && timeinfo.tm_wday <= 5) && 
      ((secondsSinceMidnight >= 8*3600 + 30*60) && (secondsSinceMidnight < 16*3600 + 35*60 ))) 
    {
      DBG_VERBOSE ("Market Open.");
      // These functions only update page if page is active.
      updateQuotes();
      updateGraph("ACN");
    }
    else {
      DBG_VERBOSE("Market Closed.");
    }
    
    // Update the Nexion clock at 2am, but only one time.
    if (timeSetDay != timeinfo.tm_mday && timeinfo.tm_hour == 2 && timeinfo.tm_min == 0 && timeinfo.tm_sec == 0) {
      setNexionTime();
      DBG_INFO("Updated the time on the Nexion");
      timeSetDay = timeinfo.tm_mday; // Limit update to once per day.
    }

    // Dim the Nexion overnight. Could probably even shut it down, or tie it into the office lighting.
    if (timeinfo.tm_hour >= 23 || timeinfo.tm_hour <= 6) {
      setNextionBrightness(2);
    }
  }

}
