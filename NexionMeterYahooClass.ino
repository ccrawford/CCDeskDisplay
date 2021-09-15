/*
 * Nexion Control Panel.
 * Chris Crawford, September 2021
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
// #include "yahoo_cert.h"
#include <ArduinoJson.h>
#include "YahooFin.h"
#include "CCSecrets.h" //Tokens, etc.

// restClient is used to make requests from the HomeAssistant server to control the Sonos.
RestClient restClient = RestClient("192.168.1.207", 8123);



EasyNex myNex(Serial2);

// MQTT not yet hooked up.
const char* mqttServer = "192.168.1.207";
const int mqttPort = 1883;

#define ARDUINOJSON_USE_LONG_LONG 1
#define ARDUINOJSON_USE_DOUBLE 1

// Size from https://arduinojson.org/v6/assistant/
// DynamicJsonDocument doc(6144);

// Long-lived bearer token generated over in HA.
// const char* HA_TOKEN = "was rotated. Get this from HA.";



const char* ssid = STASSID;
const char* password = STAPSK;

//paramters define

//the definiens of hardware spi mode as follow:
//if the IC model is known or the modules is unreadable,you can use this constructed function

void getQuote(String symbol, String field)
{
  YahooFin yf = YahooFin(symbol);  
  yf.getQuote();

  char quote_msg[26];

  sprintf(quote_msg, "$%.2f($%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);

//  String quote_msg = String(yf.regularMarketPrice, 2) + " (" + 
//      String(yf.regularMarketChange,2) + "/" +
//      String (yf.regularMarketChangePercent,2) + ")";
  
  myNex.writeStr(field + ".txt", quote_msg);
  
  if (yf.regularMarketChange < 0) myNex.writeNum(field + ".pco", 63488);
  else myNex.writeNum(field + ".pco", 34784);
    
}

void getGraphData(String symbol, String field)
{
  char quote_msg[26];
  YahooFin yf = YahooFin(symbol);
  yf.getQuote();
  yf.getChart();

  // Clear the graph...we can think about adding on to it later. 
  myNex.writeStr("cle 2,0");
  myNex.writeStr("cle 2,1");

  // Change the line color based on up/down
  if(yf.regularMarketChange<0) myNex.writeNum("s0.pco0", 63488);
  else myNex.writeNum("s0.pco0", 34784);


//  Serial.print("Prev close: "); Serial.println(yf.regularMarketPreviousClose);

  String pc = "add 2,1," + String(map((long)(yf.regularMarketPreviousClose*100), 33400,34800,0,255));
  
  Serial.print("Found points: ");Serial.println(yf.minuteDataPoints);
  
  int high = 0;
  int highI = 0;
  int low = 999;
  int lowI = 0;

  for(int i = 0; i < yf.minuteDataPoints; i++)
  {
    
    if(yf.minuteQuotes[i] > 0) {
//      long ival = (long)(yf.minuteQuotes[i]*100);
      long mappedVal = map((long)(yf.minuteQuotes[i]*100), 33400,34800,0,255);

      if(mappedVal >= high) {
        high = mappedVal;
        highI = i;
      }

      if(mappedVal <= low) {
        low = mappedVal;
        lowI = i;
      }
      
      myNex.writeStr("add 2,0," + String(mappedVal));
      myNex.writeStr(pc);
    }
  }

//  myNex.writeStr("thi.txt", String(yf.regularMarketDayHigh));
  Serial.println(myNex.readNumber("dp"));
  delay(7);

  char controlDesc[50];
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", highI, 255-high-4, yf.regularMarketDayHigh);
  
  
  //Serial.print("new control:");  Serial.println(controlDesc);
  myNex.writeStr(String(controlDesc));
  
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", lowI, 255-low+26, yf.regularMarketDayLow);
  myNex.writeStr(String(controlDesc));
  
  //myNex.writeStr("xstr 231,1,88,30,0,59164,0,0,1,3,\"" + String(yf.regularMarketDayHigh) + "\"");
  //myNex.writeStr("t3.txt", String(yf.regularMarketDayLow));
  //myNex.writeStr("t2.txt", String(yf.regularMarketPreviousClose));
  
  sprintf(controlDesc, "xstr %d,%d,88,26,0,59164,0,0,1,3,\"%.2f\"", yf.minuteDataPoints, 255-map((long)(yf.regularMarketPreviousClose*100), 33400,34800,0,255), yf.regularMarketPreviousClose);
  myNex.writeStr(String(controlDesc));

  
  myNex.writeNum("t2.y", 255-map((long)(yf.regularMarketPreviousClose*100), 33400,34800,0,255));

  sprintf(quote_msg, "%.2f(%.2f/%.2f%%)",yf.regularMarketPrice, yf.regularMarketChange, yf.regularMarketChangePercent*100);
  if (yf.regularMarketChange < 0) myNex.writeNum("t1.pco", 63488);
  else myNex.writeNum("t1.pco", 34784);
  myNex.writeStr("t1.txt", quote_msg);
  
  return;

}

void selectSource(String channelName) {
  char postParameter[80];
  String postValue = "{\"entity_id\":\"media_player.sonos_5\", \"source\":\"" + channelName + "\"}";
  postValue.toCharArray(postParameter, 80);

  restClient.setHeader(HA_TOKEN);

  int statusCode = restClient.post("/api/services/media_player/select_source", postParameter);
  Serial.println(statusCode);
}

void mediaControl(const char * command) {
  char postParameter[37];
  String postValue = "{\"entity_id\":\"media_player.sonos_5\"}";
  
  
  postValue.toCharArray(postParameter, 37);

  restClient.setHeader(HA_TOKEN);
  char service[100];
  strcat(service, "/api/services/media_player/");
  strcat(service, command);
  int statusCode = restClient.post(service, postParameter);
  Serial.println(statusCode);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("YahooNexionStockTicker Sep 2021"));

  myNex.begin(9600);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
//  Serial.println("");
//  Serial.println("WiFi connected");
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
    
  updateQuotes();

  getGraphData("ACN", "s0");

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
  Serial.println("Trigger0!");
  mediaControl("media_play_pause");
}

void trigger4() {
  Serial.println("Trigger4!");
  mediaControl("volume_up");
}

void trigger3() {
  Serial.println("Trigger3!");
  mediaControl("volume_down");
}

void trigger5() {   //Can be deleted. Use 00.
  Serial.println("Trigger5!");
  mediaControl("media_play_pause");
}

void trigger1() {
  Serial.println("Trigger1!");

  char postParameter[37];
  String postValue = "{\"entity_id\":\"light.office_light\"}";
  postValue.toCharArray(postParameter, 37);

  restClient.setHeader(HA_TOKEN);

  int statusCode = restClient.post("/api/services/light/toggle", postParameter);
  Serial.println(statusCode);
}


void trigger2() {
  Serial.println("Trigger2");
  selectSource("WXRT Over the Air");
}

void trigger6() {
  Serial.println("Trigger6!");
  selectSource("Discover Weekly");
}

void trigger16() {
  Serial.println("Trigger10");
  updateQuotes();
}

void trigger17() {
    Serial.println("Trigger17");
    getGraphData("ACN", "s0");
}

unsigned long lastRefresh;
unsigned long lastClock;

time_t now;
char strftime_buf[64];
struct tm timeinfo;

void loop() {
  
  myNex.NextionListen();

  // Refresh every two minutes when market is open.
  if((millis() - lastRefresh) > 120000) {
    lastRefresh = millis();

//      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
      
        
      Serial.println(timeinfo.tm_wday);
      Serial.println(timeinfo.tm_hour);
      if((timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6) && ((timeinfo.tm_hour > 8 || (timeinfo.tm_hour==8 && timeinfo.tm_min >29)) && timeinfo.tm_hour < 15))
      {
        Serial.println("Market Open.");
        updateQuotes();
       }
      else {
        Serial.println("Market Closed.");
      }
    }
  
  if(millis() - lastClock > 1000) {
    // Serial.println(millis());
    if (!getLocalTime(&timeinfo)){
      Serial.println("Couldn't get local time");
    }
    else {
      myNex.writeStr("t6.txt",asctime(&timeinfo));
    }
    lastClock = millis();
  }

}
