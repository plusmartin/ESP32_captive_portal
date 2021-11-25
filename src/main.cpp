
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>  //para guardar datos no volatiles (wifi, currenttime) https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/

//SSIDs to be presented to the user
String SSID1 = "";
String SSID2 = "";
String SSID3 = "";
String SSID4 = "";
String SSID5 = "";

#define WIFI_CONNECTION_ATTEMPTS 2

Preferences preferences;
DNSServer dnsServer;
AsyncWebServer server(80);

String ssid;
String password;
String saved_ssid;
String saved_password;

bool valid_ssid_received = false;
bool valid_password_received = false;
bool wifi_timeout = false;
int flag_online=0;
int flag_rescanwifi=0;
IPAddress Ip(192, 168, 0, 1);    //setto IP Access Point


unsigned long currentMillis;
unsigned long previousMillis_wificheck; //para


//Webpage HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Captive Portal Demo</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <h3>Captive Portal Demo</h3>
  <br><br>
  <form action="/get">
    <br>
    SSID: <select name = "ssid">
      <option value=%SSID1%>%SSID1%</option>
      <option value=%SSID2%>%SSID2%</option>
      <option value=%SSID3%>%SSID3%</option>
      <option value=%SSID4%>%SSID4%</option>
      <option value=%SSID5%>%SSID5%</option>
    </select>
    <br>
    <br>
    Password: <input type="text" name="password">
    <input type="submit" value="Submit">
  </form>
</body></html>)rawliteral";

//Processor for adding values to the HTML
String processor(const String& var) {
  //Serial.println(var);
  if (var == "SSID1") {
    return SSID1;
  }
  else if (var == "SSID2") {
    return SSID2;
  }
  else if (var == "SSID3") {
    return SSID3;
  }
  else if (var == "SSID4") {
    return SSID4;
  }
  else if (var == "SSID5") {
    return SSID5;
  }

}

void scanWiFi()
{
  WiFi.mode(WIFI_AP_STA); 
  //WiFi.disconnect();
  delay(100);
  Serial.println("scan start");
  int k = 5;

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  if (n < k) k = n;
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
      if (i == 0)
      {
        SSID1 = WiFi.SSID(i);
      }
      else if (i == 1)
      {
        SSID2 = WiFi.SSID(i);
      }
      else if (i == 2)
      {
        SSID3 = WiFi.SSID(i);
      }
      else if (i == 3)
      {
        SSID4 = WiFi.SSID(i);
      }
      else if (i == 4)
      {
        SSID5 = WiFi.SSID(i);
      }
    }
  }
  Serial.println("");

}//end scan


class CaptiveRequestHandler : public AsyncWebHandler {
  public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
      //request->addInterestingHeader("ANY");
      return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", index_html, processor);
    }
};

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
    Serial.println("Client Connected");
    flag_rescanwifi=1;
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    String inputParam;

    if (request->hasParam("ssid")) {
      inputMessage = request->getParam("ssid")->value();
      inputParam = "ssid";
      ssid = inputMessage;
      Serial.println(inputMessage);
      valid_ssid_received = true;
    }

    if (request->hasParam("password")) {
      inputMessage = request->getParam("password")->value();
      inputParam = "password";
      password = inputMessage;
      Serial.println(inputMessage);
      valid_password_received = true;
    }
    request->send(200, "text/html", "The values entered by you have been successfully sent to the device. It will now attempt WiFi connection");
  });
}

void WiFiSoftAPSetup()
{
  scanWiFi();
  WiFi.mode(WIFI_AP);
  WiFi.softAP("esp-captive");
  delay(100);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());
}

void StartCaptivePortal() {
  Serial.println("Setting up AP Mode");
  WiFiSoftAPSetup();
  Serial.println("Setting up Async WebServer");
  setupServer();
  Serial.println("Starting DNS Server");
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP
  server.begin();
  dnsServer.processNextRequest();
}

void WiFiStationSetup(String rec_ssid, String rec_password)
{
  wifi_timeout = false;
  WiFi.mode(WIFI_STA);
  char ssid_arr[20];
  char password_arr[20];
  rec_ssid.toCharArray(ssid_arr, rec_ssid.length() + 1);
  rec_password.toCharArray(password_arr, rec_password.length() + 1);
  Serial.print("Received SSID: "); Serial.println(ssid_arr); Serial.print("And password: "); Serial.println(password_arr);
  WiFi.begin(ssid_arr, password_arr);

  uint32_t t1 = millis();

  int n_attempts_left = WIFI_CONNECTION_ATTEMPTS;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("'");
    if (millis() - t1 > 5000) //50 seconds elapsed connecting to WiFi
    {
      n_attempts_left -= 1;
      Serial.println();
      Serial.println("Timeout connecting to WiFi. The SSID and Password seem incorrect.");
      Serial.print("Number of attempts left is: "); Serial.println(n_attempts_left);
      t1 = millis();

      if (n_attempts_left == 0) {
        Serial.println();
        Serial.println("All attempts exhausted");
        valid_ssid_received = false;
        valid_password_received = false;
        StartCaptivePortal();
        wifi_timeout = true;
        break;
      }
    }
  }
  if (!wifi_timeout)
  {
    
    Serial.println("");  Serial.print("WiFi connected to: "); Serial.println(rec_ssid);
    Serial.print("IP address: ");  Serial.println(WiFi.localIP());
    
    if (rec_ssid != saved_ssid) {
      Serial.print("Updating SSID to: ");Serial.println(rec_ssid);
      preferences.putString("rec_ssid", rec_ssid);
    }

    if (rec_password != saved_password) {
      Serial.print("Updating Password to: ");Serial.println(rec_password);
      preferences.putString("rec_password", password);
    }

  }
}//end WiFiStationSetup


void wificheck()
{
  char ssid_arr[20];
  char password_arr[20];
  if (WiFi.status() != WL_CONNECTED)
  {
    flag_online=0;
    WiFi.disconnect();
    
    ssid = preferences.getString("rec_ssid", "abcd");
    password = preferences.getString("rec_password", "12345678");
    Serial.print("Reconnecting to WiFi with SSID: ");Serial.print(ssid);Serial.print(" pass:");Serial.println(password);
    ssid.toCharArray(ssid_arr, ssid.length() + 1);
    password.toCharArray(password_arr, password.length() + 1);
  }
  
  if (WiFi.status() != WL_CONNECTED) //if still offline
  {
    WiFi.disconnect();
    Serial.println("Searching for known wifi and starting portal now...");
    StartCaptivePortal();

    for(int i=60;i>0;i--) //launch portal and try to connect to previous wifi at the same time, repeat every 60 sec
    {
      WiFi.begin(ssid_arr, password_arr);
      if (WiFi.status() == WL_CONNECTED){
        i=0; flag_online=1;
        server.end();
        WiFi.mode(WIFI_STA);  //stop portal
        Serial.println("WiFi ok");
      }
      Serial.print(i); Serial.print(" ");
      dnsServer.processNextRequest();
      delay(1000);
      if (valid_ssid_received && valid_password_received)
      {
        Serial.println("Attempting WiFi Connection!");
        WiFiStationSetup(ssid, password);
        i=0;
      }
      if(flag_rescanwifi==1) //when client refreshes page do a re-scan of wifis
      {
        flag_rescanwifi=0;
        WiFi.disconnect();
        StartCaptivePortal();
      }
    }
    
    //next: end captive portal after 60 seconds unless a clients has connected to the esp32 in which case keep portal active unless clients disconnects, in which case resume 60 sec countdown.
  }
}//end wificheck



void setup() {
  //your other setup stuff...
  Serial.begin(115200);
  Serial.println();

  preferences.begin("my-pref", false);
  ssid = preferences.getString("rec_ssid", "abcd");
  password = preferences.getString("rec_password", "12345678");
  wificheck();
  //Your Additional Setup code
}//end setup

void loop()
{
  currentMillis = millis();
    if (currentMillis - previousMillis_wificheck >= 3000)
  {
    wificheck();
    previousMillis_wificheck = currentMillis;
  }
  
  Serial.print(".");
  delay(1000);


} //end loop

// //features:
// setup: attempt to connect to previously stored wifi (stored in prefefrences, the non volatile memory)
// if no credentials stored then run routine 1.

// loop:
// 0. check if wifi credentialas are available. 
// if credentials available try to connect to wifi for 60 seconds, (for example 60 attempts each second)
// if unsuccessful wifi connection then run routine 1.
// if no credentials available then run routine 1.
// 1. if no wifi credentials stored in preferences then scan for wifis, then launch captive portal and wait for entry until timeout of 60 seconds
// inside captive portal add a re-scan button (im not sure how to scan for wifis while in AP mode, maybe it is WIFI_STA_AP)
// when a credential is received, store it in prefefrences and go to step 0.
// if portal timeout then run routine 0.
// 



//high level:
//1. if no stored wifi credentials launch portal 
//2. if stored credentials try to connect to wifi for x seconds
//3. if wifitimeout then launch portal for 60 seconds
//4. if during portal new credentials added then end portal and connect to wifi (step 2)
//5. if portaltimeout then end portal and connect to wifi (step 2)