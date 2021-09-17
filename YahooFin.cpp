#include "Arduino.h"
#include "YahooFin.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "yahoo_cert.h"
#include <ArduinoJson.h>

YahooFin::YahooFin(char* symbol)
{
  _symbol = symbol;
}

bool YahooFin::isMarketOpen()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
    Serial.println("Couldn't get local time");
  }
  return ((timeinfo.tm_wday > 0 && timeinfo.tm_wday < 6) 
      && ((timeinfo.tm_hour > 8 || (timeinfo.tm_hour==8 && timeinfo.tm_min >=30)) 
      && timeinfo.tm_hour < 15));
}


void YahooFin::getQuote(){
  HTTPClient client;

  client.useHTTP10(true);
  
  DynamicJsonDocument doc(6144);

  char url[80];
  sprintf(url, "https://query1.finance.yahoo.com/v10/finance/quoteSummary/%s?modules=price",_symbol);
    
  client.begin(url, cert_DigiCert_SHA2_High_Assurance_Server_CA);
  int httpCode = client.GET();

  if (httpCode > 0) {
    Serial.println(httpCode);
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
    
  }
  else {
    Serial.println("Error on HTTP request");
  }

  doc.clear();
  client.end();
}

void YahooFin::getChart(){
   HTTPClient client;
   client.useHTTP10(true);
   
   DynamicJsonDocument doc(8192);
   Serial.print("Doc capacity: ");Serial.println(doc.capacity());
   
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
       Serial.println("Failed to parse response to JSON with " + String(err.c_str()));
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
    Serial.println("Error on HTTP request");
    client.end();
  }
  doc.clear();
}
