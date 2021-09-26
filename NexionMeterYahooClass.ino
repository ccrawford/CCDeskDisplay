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
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <EasyNextionLibrary.h>
#include <RestClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "YahooFin.h"
#include "CCSecrets.h" //Tokens, passwords, etc.

// restClient is used to make requests from the HomeAssistant server to control the Sonos.
RestClient restClient = RestClient("192.168.1.207", 8123);

EasyNex myNex(Serial2);

// MQTT not yet hooked up.
const char* mqtt_server = "192.168.1.207";
WiFiClient (espClient);
PubSubClient client(espClient);

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_USE_DOUBLE 1

const char* ssid = STASSID;
const char* password = STAPSK;


// MQTT callback for media message
void callback(char* topic, byte* payload, unsigned int length) {

  if(myNex.readNumber("dp") != 3) {
    Serial.println("Not on page3, skipping updating media player info.");
    return;
  }

//  char quote_msg[26];
//  sprintf(quote_msg, "$%.2f($%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
 
//  myNex.writeStr(field + ".txt", quote_msg);
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if(!strcmp(topic, "homeassistant/media_player/volume")) {
    Serial.print("Volume: "); 
  }


  if(!strcmp(topic, "homeassistant/media_player/track")) {
    if (length>100) length=100;
    char track[101];
    strncpy(track, (char *)payload, length);
    track[length] = 0;
    Serial.print("track: "); Serial.println(track);
    myNex.writeStr("t0.txt", track);
  }
  if(!strcmp(topic, "homeassistant/media_player/state")) {
    Serial.print("State: "); 
  }
  if(!strcmp(topic, "homeassistant/media_player/artist")) {
    if (length>100) length=100;
    char artist[101];
    strncpy(artist, (char *)payload, length);
    artist[length] = 0;
    Serial.print("Artist: "); Serial.println(artist);
    myNex.writeStr("t1.txt", artist);
  }
 
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect("DesktopBuddy","hass.mqtt","trixie*1")) {
      Serial.println("connected");
      client.subscribe("homeassistant/media_player/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// Update the RTC in the Nextion. Actual time dispaly handled over there. 
void setTime()
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

  char quote_msg[26];
  sprintf(quote_msg, "$%.2f($%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
 
  myNex.writeStr(field + ".txt", quote_msg);
  
  if (yf.regularMarketChange < 0) myNex.writeNum(field + ".pco", 63488);
  else myNex.writeNum(field + ".pco", 34784);
    
}

void updateGraph(char * symbol)
{
  if(myNex.readNumber("dp") != 2) {
    Serial.println("Not on page2, skipping graph stuff.");
    return;
  }

  // Update the detailed quote on page.
  char quote_msg[26];
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
  Serial.println(F("YahooNexionStockTicker Sep 2021"));

  myNex.begin(115200);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // Set time via NTP, as required for x.509 validation
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  setTime();    

  // Setup MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Update quote info
  updateQuotes();
  updateGraph("ACN");

  Serial.println("=================DONE=================");
}



void updateQuotes()
{
  if(myNex.readNumber("dp") != 0) {
    Serial.println("Not on page0, skipping.");
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
  mediaControl("volume_down");
}

void trigger4() {
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

unsigned long lastRefresh;
unsigned long lastClock;

char strftime_buf[64];
struct tm timeinfo;

void loop() {
  
  myNex.NextionListen();

  // Do MQTT checks
  if (!client.connected()) {
    Serial.println("reconnecting...");
    reconnect();
  }
  client.loop();


  // Refresh every two minutes when market is open.
  if((millis() - lastRefresh) > 120000) {
    lastRefresh = millis();
    if (!getLocalTime(&timeinfo)) Serial.println("Couldn't get local time");
    
    if((timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6) && ((timeinfo.tm_hour > 8 || (timeinfo.tm_hour==8 && timeinfo.tm_min >29)) && timeinfo.tm_hour < 15))
    {
      Serial.println("Market Open.");
      // These functions only update page if page is active.
      updateQuotes();
      updateGraph("ACN");
     }
    else {
      Serial.println("Market Closed.");
    }
  }

}
