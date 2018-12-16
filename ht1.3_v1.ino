#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <ArduinoJson.h>
#include <FirebaseArduino.h>
#include <EEPROM.h>
//#include "SSD1306Wire.h"
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "WiFiManager.h"
#include "ESP8266WiFi.h"
#include "DHT.h"

#define TZ              6       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define DHTPIN D3
#define DHTTYPE DHT22
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
#define FIREBASE_HOST "humidtemp-59706.firebaseio.com"
#define FIREBASE_AUTH "i0FITeXlp0SgwgubVa580b3Zy42KX0Lw1FXDV3rB"   //Database Secret

String UNIT = "B01";
const char* wifiName = "HT-B01";      
long time_pubFirebase=10*1000;     //60s
long time_LogFirebase=15*1000;    //10*60*1000;  //10 min
long time_showData=10*1000;    //30*1000;        //30s
long time_lcdOff=20*1000;                      // 2*60*1000;          //120s
long time_getDHT=10*1000;          //10s
const int UPDATE_INTERVAL_SECS = 15;   //20 * 60; // Update every 20 minutes

time_t now;
DHT dht(DHTPIN, DHTTYPE);

const int interruptPin = 15;//D8 [3]
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D1;   //D3;
const int SDC_PIN = D2;  //D4;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif

int first=1,i=0,maxNetfound = 0;
long time_start,time_start1,time0=0,time1=0,time2=0,time3=0;
int stage = 0;

bool staLCD = true,staHospot = false,staX = true;
int train=0,rest=0;
float temp=0,humid=0,hic=0;
String flag,water;
float iTemp,iHumid,FBiTemp,FBiHumid;

bool readyForWeatherUpdate = false;
String lastUpdate = "--";
long timeSinceLastWUpdate = 0;

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

//SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);  //SSD1306Wire  display(0x3c, 5,4);
SH1106Wire  display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);  //SSD1306Wire  display(0x3c, 5,4);
OLEDDisplayUi   ui( &display );
OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void updateDataNoDisplay(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentHumid(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentFlag(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();
void drawDetail();
String dateTime();

FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawCurrentHumid,drawCurrentFlag, drawForecast };
int numberOfFrames = 5;
OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;
 
void setup() {
  Serial.begin(115200);
  
  dht.begin();
  
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
  EEPROM.begin(512);
  iTemp = EEPROM_read(0, 5).toFloat();
  iHumid = EEPROM_read(10, 5).toFloat();
  Serial.print("EEPROM iTemp::");  Serial.println(iTemp);
  Serial.print("EEPROM iHumid::");  Serial.println(iHumid);
    
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterruptHospot, RISING); //RISING  

  display.init();
  display.clear();  
  display.display();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);
  
  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);
  //ui.setIndicatorPosition(BOTTOM);  // TOP, LEFT, BOTTOM, RIGHT
  //ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);
  //ui.disableIndicator();
  ui.enableIndicator();
  ui.init();

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  
  time0 = millis();
  time1 = millis();
  time2 = millis();

}

void loop() {
  
  while(((millis()-time0)< time_showData) && stage!=0){
    if(staHospot == true){
      
      drawHospot(&display);   
      Serial.println("hospot mode");
      
      WiFiManager wifiManager;
      wifiManager.autoConnect("HT-B01");
      //wifiManager.autoConnect(wifiName);
      Serial.println("connected...yeey :)");
      
      staHospot = false;
      time0 = millis();
      stage = 0;
    }
  
    staX = false;
    Serial.print("show datail.......");  Serial.println(millis()-time0);

    Serial.println(stage);
    
    getDHT();
    if(stage==1){
      drawDetail(&display);
    }        
    if(stage==2){
      drawDetail2(&display);  
    }

    delay(200);

//    if(WiFi.status() != WL_CONNECTED){
//      Serial.println("hospot mode");
//      drawHospot(&display);   
//      wifiManager.autoConnect(wifiName);
//      Serial.println("connected...yeey :)");
//      staShowData = false;
//      time0 = millis();
//    }
    
  }

  
  if(staX==false){
    Serial.println("Out from Show data 45s");
    staX=true;
  }

  
  
  Serial.println(millis());
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("");
    Serial.print("online");
    if(first==1){
      Serial.println("connection FIRST 1st");
      drawProgress(&display, 10, "Internet connected..");
      delay(200);
      drawProgress(&display, 30, "Updating GPS...");    
      maxNetfound = WiFi.scanNetworks();
      updateGPS(); 
      drawProgress(&display, 70, "Updating weather...");   
      updateData(&display);          
      first=0;
      
      drawProgress(&display, 80, "Read EEROM");
      FBiTemp = Firebase.getFloat("ID/"+UNIT+"/Current/iTemp");
      FBiHumid = Firebase.getFloat("ID/"+UNIT+"/Current/iHumid");     
      if(iTemp != FBiTemp){
        EEPROM_write(0, String(FBiTemp));
        iTemp = FBiTemp;
        Serial.print("Update iTemp: ");  Serial.println(iTemp);  
      }
      if(iHumid != FBiHumid){
        EEPROM_write(10, String(FBiHumid));
        iHumid = FBiHumid;
        Serial.print("Update iHumid: ");  Serial.println(iHumid);  
      }
                
      drawProgress(&display, 100, "Done...");

      time0 = millis();
      time1 = millis();
      time2 = millis();
      
    }

    //-------------online-loop-----------------------------
    //Serial.println("online loop");   
    if((millis() - time1 > time_pubFirebase)){      
      Serial.print("publish Firebase.......");  Serial.println(millis() - time1);
      pubFirebase(UNIT);

      FBiTemp = Firebase.getFloat("ID/"+UNIT+"/Current/iTemp");
      FBiHumid = Firebase.getFloat("ID/"+UNIT+"/Current/iHumid");     
      if(iTemp != FBiTemp){
        EEPROM_write(0, String(FBiTemp));
        iTemp = FBiTemp;
        Serial.print("Update iTemp: ");  Serial.println(iTemp);  
      }
      if(iHumid != FBiHumid){
        EEPROM_write(10, String(FBiHumid));
        iHumid = FBiHumid;
        Serial.print("Update iHumid: ");  Serial.println(iHumid);  
      }
      
      time1 = millis();
    
    }
    if((millis() - time2 > time_LogFirebase)){
      Serial.print("log Firebase......");    Serial.println(millis() - time2);
      logFirebase(UNIT);
      time2 = millis();
    
    }
    
    if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
      Serial.print("INTERVAL_SECS ");   Serial.println(millis() - timeSinceLastWUpdate);
      setReadyForWeatherUpdate();
      timeSinceLastWUpdate = millis();
    }

    if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
      Serial.print("frameState ");
      updateDataNoDisplay(&display);
    }
    
  }

  //----------------Both-loop------------------------------------------------------
  //Serial.println("Both loop");
  if((millis() - time3 > time_getDHT)){      
      Serial.print("get DHT......");  Serial.println(millis() - time3);
      getDHT();
     
      time3 = millis();
    
  }
  
  if((millis() - time0 > time_lcdOff) && (staLCD == true)){
    Serial.print("1 min past");
    display.displayOff(); 
    time0 = millis();
    staLCD = false;
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {

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
  //currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_/ID);
  currentWeatherClient.updateCurrentByGPS(&currentWeather, OPEN_WEATHER_MAP_APP_ID, String(latitude,6),String(longitude,6));
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  //forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
  forecastClient.updateForecastsByGPS(forecasts, OPEN_WEATHER_MAP_APP_ID, String(latitude,6),String(longitude,6), MAX_FORECASTS);
  Serial.print("forcase::::::::"); Serial.println(forecastClient.updateForecastsByGPS(forecasts, OPEN_WEATHER_MAP_APP_ID, String(latitude,6),String(longitude,6), MAX_FORECASTS));
  
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}

void updateDataNoDisplay(OLEDDisplay *display) {
  Serial.println("update data");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  //currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_/ID);
  currentWeatherClient.updateCurrentByGPS(&currentWeather, OPEN_WEATHER_MAP_APP_ID, String(latitude,6),String(longitude,6));
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  //forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);
  forecastClient.updateForecastsByGPS(forecasts, OPEN_WEATHER_MAP_APP_ID, String(latitude,6),String(longitude,6), MAX_FORECASTS);

  if(WiFi.scanNetworks() > maxNetfound){
    Serial.println(" ");
    Serial.println("Update update GPS");
    Serial.print(maxNetfound); Serial.print("  lat: "); Serial.print(latitude); Serial.print("  long: "); Serial.println(longitude);
    updateGPS();
    maxNetfound = WiFi.scanNetworks();
  }
  else{
    Serial.println("Not update GPS ");
    Serial.print("Max network found: "); Serial.print(maxNetfound); Serial.print("  lat: "); Serial.print(latitude); Serial.print("  long: "); Serial.println(longitude);
    
  }
  readyForWeatherUpdate = false;
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

void drawDetail(OLEDDisplay *display){
  int x=0,y=0;

  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];
  
  sprintf_P(buff, PSTR("%02d/%02d/%04d"),timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0, 0, "Date: "+String(buff) );
  display->drawString(0, 16, "Temp: "+String(temp)+"°C");
  display->drawString(0, 32, "Humid: "+String(humid)+"%");
  display->drawString(0, 48, "Flag: "+flag);  
  display->display();
    
}

void drawDetail2(OLEDDisplay *display){
  int x=0,y=0;

//  now = time(nullptr);
//  struct tm* timeInfo;
//  timeInfo = localtime(&now);
//  char buff[16];
//  
//  sprintf_P(buff, PSTR("%02d/%02d/%04d"),timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(0, 0, "HIC: "+String(hic)+"°C" );
  display->drawString(0, 16, "Train: "+String(train)+" min");
  display->drawString(0, 32, "Rest: "+String(rest)+" min");
  display->drawString(0, 48, "Water: "+String(water)+" L/hr");  
  display->display();
    
}

void drawHospot(OLEDDisplay *display){
  int x=0,y=0;

//  now = time(nullptr);
//  struct tm* timeInfo;
//  timeInfo = localtime(&now);
//  char buff[16];
//  
//  sprintf_P(buff, PSTR("%02d/%02d/%04d"),timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  //display->drawString(0, 0, "Date: "+String(buff) );
  //display->drawString(0, 0, "Wifi Hospot..HT-"+UNIT );
  display->drawString(0, 16, "      Wifi Hospot..");
  display->drawString(0, 32, "          HT-"+UNIT);
 // display->drawString(0, 48, "Flag: "+flag);  
  display->display();
    
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->drawString(60 + x, 5 + y, String(temp, 2)+"°C");

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);

  
}

void drawCurrentHumid(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "Humidity");

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->drawString(50 + x, 5 + y, String(humid,2)+"%");

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(22 + x, 0 + y, currentWeather.iconMeteoCon);
  
}

void drawCurrentFlag(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
    
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  //display->drawString(64 + x, 38 + y, currentWeather.description);
  display->drawString(64 + x, 38 + y, "Flag Training");

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->drawString(50 + x, 5 + y, flag);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(22 + x, 0 + y, currentWeather.iconMeteoCon);
 
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

  String tempA = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, String(tempA));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, flag);
  
  display->setTextAlignment(TEXT_ALIGN_RIGHT);   
  display->drawString(128, 54, String(temp)+"°C");
  
  display->drawHorizontalLine(0, 52, 128);
    
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void handleInterruptHospot() {
  Serial.println("interupt....");
  time0 = millis();

  if(staLCD == true){
    if(stage==0){
    stage = 1;
    }
    else if(stage==1){
      stage = 2;
    }
    else if(stage==2){
      stage = 0;
    }
  }
  
  if(staLCD == false){    
    display.displayOn();
    staLCD = true;
  }
  
  time_start = millis();
  while(digitalRead(interruptPin)==1){
    if(millis()-time_start > 3000){
      staHospot = true;
      break;
    }
  } 
  
}

void updateGPS(){
  char bssid[6];
  DynamicJsonBuffer jsonBuffer;  
  
  Serial.println("scan start");  
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
  Serial.println(latitude);
  Serial.print("Longitude = ");
  Serial.println(longitude,6);
  Serial.println(longitude);
  
}

void getDHT(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    i++;
    if(i>30){
      temp = 0;
      humid = 0;
      i=0;
      temp = currentWeather.temp;
      humid = currentWeather.humidity;
    }    
  }
  else{
    temp = t;
    humid = h;
    
  }
  
  calFlag(temp,humid);
  
 
}

void calFlag(float t,float h){
  
  hic = dht.computeHeatIndex(t,h, false);
  if(hic < 27){       flag = "White";   water = "1/2"; train = 60; rest = 0; }
  else if(hic <= 32){ flag = "Green";   water = "1/2"; train = 50; rest = 10; }
  else if(hic <= 39){ flag = "Yellow";  water = "1";   train = 45; rest = 15; }
  else if(hic <= 51){ flag = "Red";     water = "1";   train = 30; rest = 30; }
  else {              flag = "Black";   water = "1";   train = 20; rest = 40; }

  Serial.print("Temperature: ");  Serial.print(temp);  Serial.println(" *C "); 
  Serial.print("Humidity: ");     Serial.print(humid); Serial.println(" %");
  Serial.print("Heat index: ");   Serial.print(hic);   Serial.println(" *C ");
  Serial.print("Heat index: ");   Serial.println(hic);
  Serial.print("water: ");        Serial.println(water);
  Serial.print("train: ");        Serial.println(rest);
  

}

String dateTime(){
  //20181117-01:48:51
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];

  sprintf_P(buff, PSTR("%04d%02d%02d-%02d:%02d:%02d"),timeInfo->tm_year + 1900,timeInfo->tm_mon+1,timeInfo->tm_mday, timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  

  return String(buff);
  
}

void pubFirebase(String unit_id){     //unit_id: A01,A02,B01,.. 
 
  //String UnitName = Firebase.getString("ID/"+unit_id+"/Current/UnitName");
  //float Latitude = Firebase.getFloat("ID/"+unit_id+"/Current/Latitude");
  //float Longtitude = Firebase.getFloat("ID/"+unit_id+"/Current/Longtitude");

  Serial.println("Publish Firebase");
  //Serial.print("pub Humid::");  Serial.println(humid); Serial.println(iHumid);
  //Serial.print("pub Temp::");  Serial.println(temp); Serial.println(iTemp);
  Firebase.setString("ID/"+unit_id+"/Current/DateTime", dateTime());
  Firebase.setFloat("ID/"+unit_id+"/Current/Temperature", temp+iTemp);
  Firebase.setFloat("ID/"+unit_id+"/Current/Humid", humid+iHumid);
  Firebase.setFloat("ID/"+unit_id+"/Current/HID", hic);
  Firebase.setString("ID/"+unit_id+"/Current/Flag", flag);
  Firebase.setInt("ID/"+unit_id+"/Current/Train", train);
  Firebase.setInt("ID/"+unit_id+"/Current/Rest", rest);
  Firebase.setString("ID/"+unit_id+"/Current/Water", water);
  Firebase.setFloat("ID/"+unit_id+"/Current/Latitude", latitude);
  Firebase.setFloat("ID/"+unit_id+"/Current/Longtitude", longitude);    
 
  if (Firebase.failed()) {
      Serial.print("set /number failed:");
      Serial.println(Firebase.error());  
      return;
  }
    
}

void logFirebase(String unit_id){

     Serial.print("logFirebase............"); 
     String timeStamp = dateTime();
     Firebase.setString("ID/"+unit_id+"/Log/"+timeStamp+"/DateTime", dateTime());
     Firebase.setFloat("ID/"+unit_id+"/Log/"+timeStamp+"/Temperature", temp+iTemp);
     Firebase.setFloat("ID/"+unit_id+"/Log/"+timeStamp+"/Humid", humid+iHumid);
     Firebase.setFloat("ID/"+unit_id+"/Log/"+timeStamp+"/HID", hic);
     Firebase.setString("ID/"+unit_id+"/Log/"+timeStamp+"/Flag", flag);
     Firebase.setInt("ID/"+unit_id+"/Log/"+timeStamp+"/Train", train);
     Firebase.setInt("ID/"+unit_id+"/Log/"+timeStamp+"/Rest", rest);
     Firebase.setString("ID/"+unit_id+"/Log/"+timeStamp+"/Water", water);
     Firebase.setFloat("ID/"+unit_id+"/Log/"+timeStamp+"/Latitude", latitude);
     Firebase.setFloat("ID/"+unit_id+"/Log/"+timeStamp+"/Longtitude", longitude); 
     Serial.println("Finish"); 

}

String EEPROM_read(int index, int length) {
  String text = "";
  char ch = 1;

  for (int i = index; (i < (index + length)) && ch; ++i) {
  if (ch = EEPROM.read(i)) {
  text.concat(ch);
  }
  }
  return text;
}

int EEPROM_write(int index, String text) {
  for (int i = index; i < text.length() + index; ++i) {
  EEPROM.write(i, text[i - index]);
  }
  EEPROM.write(index + text.length(), 0);
  EEPROM.commit();
  
  return text.length() + 1;
}
