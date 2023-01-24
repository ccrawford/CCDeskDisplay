#include "Arduino.h"

#ifndef YahooFin_h
#define YahooFin_h

class YahooFin
{
  public:
    YahooFin(char* symbol);
    bool isMarketOpen();
    bool isChangeInteresting();
    void getQuote();
    void getChart();
    double openPrice;
    double regularMarketPrice;
    double regularMarketDayHigh;
    double regularMarketDayLow;
    double regularMarketChangePercent;
    double regularMarketChange;
    double regularMarketPreviousClose;
    double minuteQuotes[195];
    int minuteDataPoints;
    long lastUpdateTime;
    
  private:
    char* _symbol;
};

#endif
