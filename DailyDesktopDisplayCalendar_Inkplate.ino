#ifndef ARDUINO_INKPLATECOLOR
#error "Wrong board selection for this example, please select Soldered Inkplate 6COLOR in the boards menu."
#endif

#include <Arduino.h>
#include "Inkplate.h" // Include Inkplate library to the sketch
#include "include/Image.h"

#include <WiFi.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#include <SPI.h>
 
const char rootPath[] = "/images/"; // NOTE: Must end with /

Inkplate display;     

struct Config {
  char wifiSSID[64];
  char wifiPassword[64];
  char ntpServer[64];
  int TimzoneOffsetSeconds;
};

const char *configFilename = "/D3Config.json";  
Config config; 

#define uS_TO_S_FACTOR 1000000 
const long  gmtOffset_sec = 0;

const uint8_t low_battery_icon[] PROGMEM = {
0x80,0x0,0x70,
0x7f,0xff,0xb0,
0x5f,0xff,0xb0,
0x5f,0xff,0x80,
0x4f,0xff,0xa0,
0x4f,0xff,0xa0,
0x47,0xff,0xa0,
0x47,0xff,0xa0,
0x43,0xff,0x80,
0x43,0xff,0xb0,
0x7f,0xff,0xb0,
0x80,0x0,0x70,
};
int low_battery_icon_w = 20;
int low_battery_icon_h = 12;

void setup()
{

    Serial.begin(115200);
    display.begin();             // Init Inkplate library (you should call this function ONLY ONCE)
    display.clearDisplay();      // Clear frame buffer of display
    display.setCursor(0, 0);     // Set the cursor on the beginning of the screen
    display.setTextColor(BLACK); // Set text color to black
    

    if (!display.sdCardInit()) {
      Serial.println("SD Card Error!");
      display.println("SD Card Error!");
      display.display();
      sleep(false);
    }

    Serial.println(F("Loading configuration..."));
    loadConfiguration(configFilename, config);
    connectToWIFI();

    SdFile rootFolder;
    if (!rootFolder.open(rootPath)) {
      Serial.println("Failed to open images folder!");
      display.println("Failed to open images folder!");
      display.println("Please verify that a folder called images");
      display.println("exists on the inserted SD card.");
      display.display();
      sleep(false);
      return;
    }
    bool loaded =true;
    if (!exploreFolder(&rootFolder)) {
      Serial.println("No available files to display.");
      display.println("No available files to display.");
      loaded = false;
    }
    
    Serial.print("battery:");
    Serial.println(display.readBattery());
    if (display.readBattery() < 3.4) {
      display.fillRect(600 - (low_battery_icon_w + 4), 0, low_battery_icon_w + 4, low_battery_icon_h + 4, INKPLATE_WHITE);
      display.drawBitmap(598 - low_battery_icon_w, 2, low_battery_icon, low_battery_icon_w, low_battery_icon_h, INKPLATE_RED);
    }
    
    display.display();
    sleep(loaded);
}

void sleep(bool loaded){
  disconnectToWIFI();

   // Turn off the power supply for the SD card
  display.sdCardSleep();

  getAndDisplayTime();
  
  uint32_t hourOfAlarm=3;//3am
  uint32_t hoursinADayOffset = 24;
  if(display.rtcGetHour() < hourOfAlarm - 1){ //leaves a grace period of hour, this is because the esp rtc is not as accurate and we may wake up before the real time slightly 
    hoursinADayOffset=0; //alarm set the same day
  }
  uint32_t offsetHours =  (hoursinADayOffset + hourOfAlarm - display.rtcGetHour()-1);
  uint32_t offsetMinutes =(60-display.rtcGetMinute());
  uint64_t secondsToWait = (offsetHours*60*60)+(offsetMinutes*60);
  
  if(!loaded) secondsToWait = 15 * 60;
  Serial.print("seconds to sleep: ");
  Serial.println(secondsToWait);
 
  esp_sleep_enable_timer_wakeup(secondsToWait * uS_TO_S_FACTOR);
 
  // Enable wakeup from deep sleep on GPIO 36 (wake button)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW);
 
  Serial.println("sleeping");
  // Put ESP32 into deep sleep (low power mode)
  esp_deep_sleep_start();
}

bool exploreFolder(SdFile *folder) {
  for (int attemptCount = 0; attemptCount < 3; attemptCount++) {
    bool result = loadFile();
    if (result) return result;
  }
  return false;
}

bool loadFile(){
  //display.print("loading file ");
  Serial.println("trying to load files ...");

  display.rtcGetRtcData(); // Get the time and date from RTC

  // uint8_t seconds = display.rtcGetSecond();  // Store senconds in a variable
  // uint8_t minutes = display.rtcGetMinute();  // Store minutes in a variable
  // uint8_t hour = display.rtcGetHour();       // Store hours in a variable
  uint8_t day = display.rtcGetDay();         // Store day of month in a variable
  //uint8_t weekday = display.rtcGetWeekday(); // Store day of week in a variable
  uint8_t month = display.rtcGetMonth()+1;     // Store month in a variable
  uint16_t year = display.rtcGetYear();       // Store year in a variable

  SdFile file;
  
  char filename[30];

  snprintf(filename,30,"%s%02d-%d-%d.jpg", rootPath,year, month, day);
  Serial.print(filename);
  if(file.open(filename,O_READ)){
    if(displayImage(&file)){
      Serial.println(" ... loaded");
      return true;
    }
    Serial.println(" ... failed to load, trying ...");
  }

  snprintf(filename,30,"%s%02d-%d-%d.png", rootPath,year, month, day);
  Serial.print(filename);
  if(file.open(filename,O_READ)){
    if(displayImage(&file)){
      Serial.println(" ... loaded");
      return true;
    }
    Serial.println(" ... failed to load, trying ...");
  }

  snprintf(filename,30,"%s%02d-%d-%d.bmp", rootPath,year, month, day);
  Serial.print(filename);
  if(file.open(filename,O_READ)){
    if(displayImage(&file)){
      Serial.println(" ... loading");
      return true;
    }
    Serial.println(" ... failed to load");
  }
  Serial.print("Failed to load any image for ");
  Serial.println(snprintf(filename,30,"%s%02d-%d-%d", rootPath,year, month, day));
  return false;
}

bool displayImage(SdFile *file) {
  int16_t byte1 = file->read();
  int16_t byte2 = file->read();
  file->rewind();
  bool result;
  if (byte1 == 0x42 && byte2 == 0x4d) {
    // it's a bitmap
    result = display.drawBitmapFromSd(file, 0, 0, 1, 0);
  } else if (byte1 == 0xff && byte2 == 0xd8) {
    // it's a JPEG
    result = display.drawJpegFromSd(file, 0, 0, 1, 0);
  } else if (byte1 == 0x89 && byte2 == 0x50) {
    // it's a PNG
    result = display.drawPngFromSd(file, 0, 0, 1, 0);
  }
  if (!result) {
    display.print("Cannot display ");
    printlnFilename(file);
    return 0;
  }
  //Serial.println(" ... displaying image");
  return 1;
}

void printNum(int value) {
  char itoaBuffer[13];
  itoa(value, itoaBuffer, 10);
  display.print(itoaBuffer);
  Serial.print(itoaBuffer);
}

void printlnFilename(SdFile *file) {
  int maxFileNameLen = 128;
  char nameBuffer[maxFileNameLen];
  file->getName(nameBuffer, maxFileNameLen);
  display.println(nameBuffer);
  //Serial.println(nameBuffer);
}


void loop() {
   
}

void connectToWIFI() {
  Serial.print("Connecting to ");
  Serial.println(config.wifiSSID);
  WiFi.begin(config.wifiSSID, config.wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.print("WiFi connected. ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("NTP get time: timezoneOffsetSeconds "+String(config.TimzoneOffsetSeconds));
/// Init and get the time
  configTime(config.TimzoneOffsetSeconds, gmtOffset_sec, config.ntpServer);
  setRTC();

}


void setRTC(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.print("NTP: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  uint16_t year = timeinfo.tm_year +10  +2; // 10 is the difference in date formats arduino starting at 1990 and Inkplate starting at 2000. The 2 is also needed for some reason.
  display.rtcSetTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec); 
  //display.rtcSetTime(1, 58, timeinfo.tm_sec); //testing
  display.rtcSetDate(timeinfo.tm_wday, timeinfo.tm_mday, timeinfo.tm_mon, year); 
  getAndDisplayTime();
}


void getAndDisplayTime()
{
    display.rtcGetRtcData(); // Get the time and date from RTC

    uint8_t seconds = display.rtcGetSecond();  // Store senconds in a variable
    uint8_t minutes = display.rtcGetMinute();  // Store minutes in a variable
    uint8_t hour = display.rtcGetHour();       // Store hours in a variable
    uint8_t day = display.rtcGetDay();         // Store day of month in a variable
    uint8_t weekday = display.rtcGetWeekday(); // Store day of week in a variable
    uint8_t month = display.rtcGetMonth()+1;     // Store month in a variable
    uint16_t year = display.rtcGetYear();       // Store year in a variable

    Serial.print("RTC: ");                                  
    printTime(hour, minutes, seconds, day, weekday, month, year); // Print the time on screen
                                             
}

void printTime(uint8_t _hour, uint8_t _minutes, uint8_t _seconds, uint8_t _day, uint8_t _weekday, uint8_t _month,
               uint16_t _year)
{
    // Write time and date info on screen
    char *wday[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    Serial.print(_hour);
    Serial.print(':');
    Serial.print(_minutes);
    Serial.print(':');
    Serial.print(_seconds);

    Serial.print(' ');

    Serial.print(wday[_weekday]);
    Serial.print(", ");
    Serial.print(_month);
    Serial.print('/');
    Serial.print(_day);
    Serial.print('/');
    Serial.println(_year);
}

void disconnectToWIFI() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  SdFile file;
  file.open(filename,O_READ);

  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  config.TimzoneOffsetSeconds = doc["TimzoneOffsetSeconds"] | -21600; //defaults to us central time, as it's me!
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0

  strlcpy(config.wifiSSID, doc["wifiSSID"] | "add_wifi_name_to_config", sizeof(config.wifiSSID));   
  strlcpy(config.wifiPassword, doc["wifiPassword"] | "add_wifi_password_to_config", sizeof(config.wifiPassword));      
  strlcpy(config.ntpServer, doc["ntpServer"] | "add_wifi_name_to_config", sizeof(config.ntpServer));   
  
  file.close();
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  //SD.remove(filename);

  // Open file for writing
  SdFile file;
  if(!file.open(filename, FILE_WRITE)){
    Serial.println(F("Failed to create file"));
    return;
  }

  StaticJsonDocument<256> doc;

  // Set the values in the document
  doc["wifiSSID"] = config.wifiSSID;
  doc["wifiPassword"] = config.wifiPassword;
  doc["ntpServer"] = config.ntpServer;
  doc["TimzoneOffsetSeconds"] = config.TimzoneOffsetSeconds;
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}
