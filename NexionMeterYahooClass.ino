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
 * URLS: https://query1.finance.yahoo.com/v8/finance/chart/ACN?interval=2m
 * https://query1.finance.yahoo.com/v7/finance/quote?lang=en-US&region=US&corsDomain=finance.yahoo.com&symbols=ACN,^GSPC,^IXIC
 * https://query1.finance.yahoo.com/v10/finance/quoteSummary/ACN?modules=price
 * 
 * https://github.com/Seithan/EasyNextionLibrary
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <EasyNextionLibrary.h>
#include <RestClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include "YahooFin.h"
#include "CCSecrets.h" //Tokens, passwords, etc.

// restClient is used to make requests from the HomeAssistant server to control the Sonos.
RestClient restClient = RestClient("192.168.1.207", 8123);

EasyNex myNex(Serial2);

// MQTT not yet hooked up.
const char* mqttServer = "192.168.1.207";
const int mqttPort = 1883;

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_USE_DOUBLE 1

const char* ssid = STASSID;
const char* password = STAPSK;


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
  myNex.writeStr("t6.txt","updating.");
  getQuote("ACN", "t1");
  myNex.writeStr("t6.txt","updating .");
  getQuote("^GSPC", "t4");        
  myNex.writeStr("t6.txt","updating  .");
  getQuote("^IXIC", "t5");
  myNex.writeStr("t6.txt","");
}

void trigger0() {
//  Serial.println("Trigger0!");
  mediaControl("media_play_pause");
}

void trigger4() {
//  Serial.println("Trigger4!");
  mediaControl("volume_up");
}

void trigger3() {
//  Serial.println("Trigger3!");
  mediaControl("volume_down");
}

void trigger5() {   //Can be deleted. Use 00.
//  Serial.println("Trigger5!");
  mediaControl("media_play_pause");
}

// Turn office light on/off.
void trigger1() {
//  Serial.println("Trigger1!");

  restClient.setHeader(HA_TOKEN);
  int statusCode = restClient.post("/api/services/light/toggle",  "{\"entity_id\":\"light.office_light\"}");
}


void trigger2() {
//  Serial.println("Trigger2");
  selectSource("WXRT Over the Air");
}

void trigger6() {
//  Serial.println("Trigger6!");
  selectSource("Discover Weekly");
}

void trigger16() {
//  Serial.println("Trigger10");
  updateQuotes();
}

//Update stock chart
void trigger17() {
//    Serial.println("Trigger17");
    updateGraph("ACN");
}

unsigned long lastRefresh;
unsigned long lastClock;

char strftime_buf[64];
struct tm timeinfo;

void loop() {
  
  myNex.NextionListen();

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
