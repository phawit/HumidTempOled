#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <ArduinoJson.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "WiFiManager.h"
#include "ESP8266WiFi.h"

#define TZ              6       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

int sta = 1,first=1;
long time_start,time_start1;
int maxNetfound = 0;

const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes
String OPEN_WEATHER_MAP_APP_ID = "81630c00c360807126a5ab5f19b221da";
String OPEN_WEATHER_MAP_LOCATION_ID = "1609350";
String OPEN_WEATHER_MAP_LANGUAGE = "en";
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

const char* Host = "www.googleapis.com";
String thisPage = "/geolocation/v1/geolocate?key=";
String key = "AIzaSyCKm8XytNFYtpfEKLbNyDDh9obrDy94ynU";
int status = WL_IDLE_STATUS;
String jsonString = "{\n";
double latitude    = 0.0;
double longitude   = 0.0;
int more_text = 0; // set to 1 for more debug output

SSD1306Wire  display(0x3c, 5,4);
OLEDDisplayUi   ui( &display );

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

bool readyForWeatherUpdate = false;
String lastUpdate = "--";
long timeSinceLastWUpdate = 0;

const int interruptPin = 15;//D8 [3]

void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();

FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast };
int numberOfFrames = 3;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterruptHospot, RISING); //RISING  

  display.init();
  display.clear();
  display.display();
  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  //WiFi.begin(WIFI_SSID, WIFI_PWD);

//  int counter = 0;
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//    display.clear();
//    display.drawString(64, 10, "Connecting to WiFi");
//    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
//    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
//    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
//    display.display();
//
//    counter++;
//  }

  

  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);
  ui.setIndicatorPosition(BOTTOM);  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);
  ui.init();

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  updateData(&display);

  
  

}

void loop() {

  if(sta==0){
    Serial.println("hospot mode");
    WiFiManager wifiManager;  
 
    wifiManager.autoConnect("HT-A03");
    Serial.println("connected...yeey :)");
    sta = 1;

    configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
    updateData(&display);

    maxNetfound = WiFi.scanNetworks();
    updateGPS();
  }

  if(WiFi.status() == WL_CONNECTED){
    if(first==1){
      updateData(&display);
      first=0;
      maxNetfound = WiFi.scanNetworks();
      updateGPS();
    }
    
  }

  
  //--------------------
//  if(WiFi.scanNetworks() > maxNetfound){
//    Serial.println("Update update GPS");
//    updateGPS();
//    maxNetfound = WiFi.scanNetworks();
//  }
//  else{
//    Serial.println("Not update GPS ");
//    Serial.print("Max network found: "); Serial.print(maxNetfound); Serial.print("  lat: "); Serial.print(latitude); Serial.print("  long: "); Serial.println(longitude);
//    
//  }

  //-------------------------------

  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    Serial.print("INTERVAL_SECS ");
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    Serial.print("frameState ");
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }


}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  Serial.println("update data");
  drawProgress(display, 10, "Updating time...");
  drawProgress(display, 30, "Updating weather...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
//  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_/ID);
  currentWeatherClient.updateCurrentByGPS(&currentWeather, OPEN_WEATHER_MAP_APP_ID, "13.6623","100.3019");
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}



void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  
  
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];


  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];

  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);

  
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
  
  
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
  
  
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void handleInterruptHospot() {
  Serial.println("interupt....");
  time_start = millis();
  while(digitalRead(interruptPin)==1){
    if(millis()-time_start > 3000){
       Serial.println("save mode");
       sta = 0;

       Serial.println("    Wifi Hospot!    ");
       Serial.println("*******HT-A03*******");
      
       break;
    }
  } 


  
}

void updateGPS(){
  char bssid[6];
  DynamicJsonBuffer jsonBuffer;  
  Serial.println("scan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found...");

  if(more_text){
    // Print out the formatted json...
    Serial.println("{");
    Serial.println("\"homeMobileCountryCode\": 234,");  // this is a real UK MCC
    Serial.println("\"homeMobileNetworkCode\": 27,");   // and a real UK MNC
    Serial.println("\"radioType\": \"gsm\",");          // for gsm
    Serial.println("\"carrier\": \"Vodafone\",");       // associated with Vodafone 
    //Serial.println("\"cellTowers\": [");                // I'm not reporting any cell towers      
    //Serial.println("],");      
    Serial.println("\"wifiAccessPoints\": [");
    for (int i = 0; i < n; ++i)
    {
      Serial.println("{");
      Serial.print("\"macAddress\" : \"");    
      Serial.print(WiFi.BSSIDstr(i));
      Serial.println("\",");
      Serial.print("\"signalStrength\": ");     
      Serial.println(WiFi.RSSI(i));
      if(i<n-1)
      {
      Serial.println("},");
      }
      else
      {
      Serial.println("}");  
      } 
    }
    Serial.println("]");
    Serial.println("}");   
  }
    Serial.println(" ");
  }    
// now build the jsonString...
jsonString="{\n";
jsonString +="\"homeMobileCountryCode\": 234,\n";  // this is a real UK MCC
jsonString +="\"homeMobileNetworkCode\": 27,\n";   // and a real UK MNC
jsonString +="\"radioType\": \"gsm\",\n";          // for gsm
jsonString +="\"carrier\": \"Vodafone\",\n";       // associated with Vodafone 
jsonString +="\"wifiAccessPoints\": [\n";
    for (int j = 0; j < n; ++j)
    {
      jsonString +="{\n";
      jsonString +="\"macAddress\" : \"";    
      jsonString +=(WiFi.BSSIDstr(j));
      jsonString +="\",\n";
      jsonString +="\"signalStrength\": ";     
      jsonString +=WiFi.RSSI(j);
      jsonString +="\n";
      if(j<n-1)
      {
      jsonString +="},\n";
      }
      else
      {
      jsonString +="}\n";  
      }
    }
    jsonString +=("]\n");
    jsonString +=("}\n"); 
//--------------------------------------------------------------------
  
 Serial.println("");

 WiFiClientSecure client;
  
  //Connect to the client and make the api call
  Serial.print("Requesting URL: ");
  Serial.println("https://" + (String)Host + thisPage + "<API_Key>");
  Serial.println(" ");
  if (client.connect(Host, 443)) {
    Serial.println("Connected");    
    client.println("POST " + thisPage + key + " HTTP/1.1");    
    client.println("Host: "+ (String)Host);
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.println("User-Agent: Arduino/1.0");
    client.print("Content-Length: ");
    client.println(jsonString.length());    
    client.println();
    client.print(jsonString);  
    delay(500);
  }

  //Read and parse all the lines of the reply from server          
  while (client.available()) {
    String line = client.readStringUntil('\r');
    if(more_text){
      Serial.print(line);
    }    
    JsonObject& root = jsonBuffer.parseObject(line);
    if(root.success()){
    latitude    = root["location"]["lat"];
    longitude   = root["location"]["lng"];
    }
  }

  Serial.println("closing connection");
  Serial.println();
  client.stop();

  Serial.print("Latitude = ");
  Serial.println(latitude,6);
  Serial.print("Longitude = ");
  Serial.println(longitude,6);
  
}

