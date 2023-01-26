#include "Arduino.h"
#include "YahooFin.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "yahoo_cert.h"
#include <ArduinoDebug.hpp>
#include <ArduinoJson.h>

YahooFin::YahooFin(char* symbol)
{
  _symbol = symbol;
  regularMarketPrice = 0;
  lastUpdateOfDayDone = false;
}

bool YahooFin::isMarketOpen()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
    DBG_ERROR("Couldn't get local time");
  }
  return ((timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6) 
      && ((timeinfo.tm_hour > 8 || (timeinfo.tm_hour==8 && timeinfo.tm_min >=30)) 
      && timeinfo.tm_hour < 15));
}

bool YahooFin::isChangeInteresting()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
    DBG_ERROR("Couldn't get local time");
  }

//  Serial.printf("Day: %d, Hour: %d, Min: %d\n",timeinfo.tm_wday, timeinfo.tm_hour, timeinfo.tm_min);

  //Change is only interesting during the trading day or in the evening after.
  return ((timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6) 
      && ((timeinfo.tm_hour > 8 || (timeinfo.tm_hour==8 && timeinfo.tm_min >=30))));  
}


void YahooFin::getQuote(){

  if (this->isMarketOpen() || regularMarketPrice == 0 || !lastUpdateOfDayDone)
  {
    lastUpdateOfDayDone = !this->isMarketOpen();
    
    HTTPClient client;
  
    client.useHTTP10(true);
    
    DynamicJsonDocument doc(6144);
  
    char url[80];
    sprintf(url, "https://query1.finance.yahoo.com/v10/finance/quoteSummary/%s?modules=price",_symbol);
      
    client.begin(url, cert_DigiCert_SHA2_High_Assurance_Server_CA);
    int httpCode = client.GET();
  
    if (httpCode > 0) {
      DBG_VERBOSE(httpCode);
      auto err = deserializeJson(doc, client.getStream());
      if (err) {
        Serial.println("Failed to parse response to JSON with " + String(err.c_str()));
      }
  
      regularMarketPrice=doc["quoteSummary"]["result"][0]["price"]["regularMarketPrice"]["raw"].as<float>(); 
      regularMarketDayHigh=doc["quoteSummary"]["result"][0]["price"]["regularMarketDayHigh"]["raw"].as<float>();
      regularMarketDayLow=doc["quoteSummary"]["result"][0]["price"]["regularMarketDayLow"]["raw"].as<float>();
      regularMarketChangePercent=doc["quoteSummary"]["result"][0]["price"]["regularMarketChangePercent"]["raw"].as<float>();
      regularMarketChange=doc["quoteSummary"]["result"][0]["price"]["regularMarketChange"]["raw"].as<float>();
      regularMarketPreviousClose=doc["quoteSummary"]["result"][0]["price"]["regularMarketPreviousClose"]["raw"].as<float>();

      time(&lastUpdateTime);
    }
    else {
      DBG_ERROR("Error on HTTP request");
    }
  
    doc.clear();
    client.end();
  }
}

void YahooFin::getChart(){
   HTTPClient client;
   client.useHTTP10(true);
   
   DynamicJsonDocument doc(8192);
   DBG_DEBUG("Doc capacity: %d", doc.capacity());
   
   StaticJsonDocument<112> filter;
   filter["chart"]["result"][0]["indicators"]["quote"][0]["close"] = true;

   char url[80];
   sprintf(url, "https://query1.finance.yahoo.com/v8/finance/chart/%s?interval=2m",_symbol); 
   client.begin(url, cert_DigiCert_SHA2_High_Assurance_Server_CA);
   int httpCode = client.GET();

   if (httpCode > 0) {
     
     auto err = deserializeJson(doc, client.getStream(), DeserializationOption::Filter(filter));
     client.end();

     if (err) {
       DBG_ERROR("Failed to parse response to JSON with " + String(err.c_str()));
     }
     JsonArray arr = doc["chart"]["result"][0]["indicators"]["quote"][0]["close"].as<JsonArray>();
     int i = 0;
     minuteDataPoints = 0;
     for (JsonVariant value : arr) {
        if(!value.isNull()) {
          if(i>=195) continue;
          minuteQuotes[i++] = value.as<double>();
          minuteDataPoints++;          
        }
     }
  }
  else {
    DBG_ERROR("Error on HTTP request");
    client.end();
  }
  doc.clear();
}
