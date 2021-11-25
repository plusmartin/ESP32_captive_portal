#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>

//SSIDs to be presented to the user
String SSID1 = "";
String SSID2 = "";
String SSID3 = "";

#define WIFI_CONNECTION_ATTEMPTS 3

Preferences preferences;
DNSServer dnsServer;
AsyncWebServer server(80);

String ssid;
String password;
String saved_ssid;
String saved_password;
bool is_setup_done = false;
bool valid_ssid_received = false;
bool valid_password_received = false;
bool wifi_timeout = false;


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

}

void scanWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("scan start");
  int k = 3;

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
    }
  }
  Serial.println("");

}


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
  Serial.print("AP IP address: "); Serial.println(WiFi.softAPIP());
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
    delay(2000);
    Serial.print(".");
    if (millis() - t1 > 50000) //50 seconds elapsed connecting to WiFi
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
        is_setup_done = false;
        preferences.putBool("is_setup_done", is_setup_done);

        StartCaptivePortal();
        wifi_timeout = true;
        break;
      }
    }
  }
  if (!wifi_timeout)
  {
    is_setup_done = true;
    Serial.println("");  Serial.print("WiFi connected to: "); Serial.println(rec_ssid);
    Serial.print("IP address: ");  Serial.println(WiFi.localIP());
    preferences.putBool("is_setup_done", is_setup_done);
    if (rec_ssid != saved_ssid) {
      Serial.println("Updating SSID");
      preferences.putString("rec_ssid", rec_ssid);
    }

    if (rec_password != saved_password) {
      Serial.println("Updating Password");
      preferences.putString("rec_password", password);
    }

  }
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

void setup() {
  //your other setup stuff...
  Serial.begin(115200);
  Serial.println();
  preferences.begin("my-pref", false);

  //preferences.clear();

  is_setup_done = preferences.getBool("is_setup_done", false);
  ssid = preferences.getString("rec_ssid", "Sample_SSID");
  password = preferences.getString("rec_password", "abcdefgh");

  saved_ssid = ssid;
  saved_password = password;

  if (!is_setup_done)
  {
    StartCaptivePortal();
  }
  else
  {
    Serial.println("Using saved SSID and Password to attempt WiFi Connection!");
    Serial.print("Saved SSID is "); Serial.println(ssid);
    Serial.print("Saved Password is "); Serial.println(password);
    WiFiStationSetup(ssid, password);
  }

  while (!is_setup_done)
  {
    dnsServer.processNextRequest();
    delay(10);
    if (valid_ssid_received && valid_password_received)
    {
      Serial.println("Attempting WiFi Connection!");
      WiFiStationSetup(ssid, password);
    }
  }

  Serial.println("All Done!");

  //Your Additional Setup code
}

void loop() {
  //Your Loop code
}
