/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer/examples/CaptivePortalAdvanced
   Built by AlexT https://github.com/tzapu
   Licensed under MIT license
   
   Modified by Brian Ejike, 2018.
 **************************************************************/

#include "wificonfig.h"

WIFIConfigParam::WIFIConfigParam(const char *custom) {
  _id = NULL;
  _placeholder = NULL;
  _length = 0;
  _value = NULL;

  _customHTML = custom;
}

WIFIConfigParam::WIFIConfigParam(const char *id, const char *placeholder, const char *defaultValue, int length) {
  init(id, placeholder, defaultValue, length, "");
}

WIFIConfigParam::WIFIConfigParam(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom) {
  init(id, placeholder, defaultValue, length, custom);
}

void WIFIConfigParam::init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom) {
  _id = id;
  _placeholder = placeholder;
  _length = length;
  _value = new char[length + 1];
  for (int i = 0; i < length; i++) {
    _value[i] = 0;
  }
  if (defaultValue != NULL) {
    strncpy(_value, defaultValue, length);
  }

  _customHTML = custom;
}

const char* WIFIConfigParam::getValue() {
  return _value;
}
const char* WIFIConfigParam::getID() {
  return _id;
}
const char* WIFIConfigParam::getPlaceholder() {
  return _placeholder;
}
int WIFIConfigParam::getValueLength() {
  return _length;
}
const char* WIFIConfigParam::getCustomHTML() {
  return _customHTML;
}

WIFIConfig::WIFIConfig() {
}

void WIFIConfig::addParameter(WIFIConfigParam *p) {
  if(_paramsCount + 1 > WIFICONFIG_MAX_PARAMS)
  {
    //Max parameters exceeded!
	WC_DEBUG_PRINTLN("WIFICONFIG_MAX_PARAMS exceeded, increase number (in WIFIConfig.h) before adding more parameters!");
	WC_DEBUG_PRINTLN("Skipping parameter with ID:");
	WC_DEBUG_PRINTLN(p->getID());
	return;
  }
  _params[_paramsCount] = p;
  _paramsCount++;
  WC_DEBUG_PRINT("::Adding parameter: ");
  WC_DEBUG_PRINTLN(p->getID());
}

void WIFIConfig::setupConfigPortal() {
  dnsServer.reset(new DNSServer());
  server.reset(new ESP8266WebServer(80));

  _configPortalStart = millis();

  WC_DEBUG_PRINT("::Configuring access point: ");
  WC_DEBUG_PRINTLN(_apName);

  if (_apPassword != NULL) {
    WiFi.softAP(_apName, _apPassword);//password option
  } else {
    WiFi.softAP(_apName);
  }

  delay(500); // Without delay I've seen the IP address blank
  WC_DEBUG_PRINT("::AP IP address: ");
  WC_DEBUG_PRINTLN(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server->on("/", std::bind(&WIFIConfig::handleRoot, this));
  server->on("/wifisave", std::bind(&WIFIConfig::handleWifiSave, this));
  server->on("/i", std::bind(&WIFIConfig::handleInfo, this));
  server->on("/r", std::bind(&WIFIConfig::handleReset, this));
  //server->on("/generate_204", std::bind(&WIFIConfig::handle204, this));  //Android/Chrome OS captive portal check.
  server->on("/fwlink", std::bind(&WIFIConfig::handleRoot, this));  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server->onNotFound (std::bind(&WIFIConfig::handleNotFound, this));
  server->begin(); // Web server start
  WC_DEBUG_PRINTLN("::HTTP server started");

}

boolean WIFIConfig::configPortalHasTimeout(){
    if(_configPortalTimeout == 0 || wifi_softap_get_station_num() > 0){
      _configPortalStart = millis(); // kludge, bump configportal start time to skew timeouts
      return false;
    }
    return (millis() > _configPortalStart + _configPortalTimeout);
}

boolean WIFIConfig::startConfigPortal() {
  String ssid = "ESP" + String(ESP.getChipId());
  return startConfigPortal(ssid.c_str(), NULL);
}

boolean WIFIConfig::startConfigPortal(char const *apName, char const *apPassword) {
  //setup AP
  WiFi.mode(WIFI_AP);
  WC_DEBUG_PRINTLN("::Setting up AP");

  _apName = apName;
  _apPassword = apPassword;

  recvd_config = false;
  config_state = WIFICONFIG_INPROGRESS;
  setupConfigPortal();

  return true;
}

uint8_t WIFIConfig::config_loop(void) {
    if (config_state == WIFICONFIG_COMPLETE || 
        config_state == WIFICONFIG_TIMEOUT ||
        config_state == WIFICONFIG_NOTSTARTED)
        return config_state;
        
    if (configPortalHasTimeout()) {
        config_state = WIFICONFIG_TIMEOUT;
        server.reset();
        dnsServer->stop();
        dnsServer.reset();
      
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    }
    
    else if (recvd_config) {
      recvd_config = false;
      delay(1000);
      WC_DEBUG_PRINTLN("::Stopping servers");
      
      server.reset();
      dnsServer->stop();
      dnsServer.reset();
      config_state = WIFICONFIG_COMPLETE;
      
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);

      /*if (_ssid != "")
        WiFi.begin(_ssid.c_str(), _pass.c_str());*/
      
      if ( _savecallback != NULL) {
          //todo: check if any custom parameters actually exist, and check if they really changed maybe
          _savecallback();
      }
    }
    else {
        dnsServer->processNextRequest();
        server->handleClient();
    }
    
    return config_state;
}

// 'len' stands for the max number of chars to copy excluding the nul byte 
bool WIFIConfig::get_wifi_ssid(char * ssidbuf, uint16_t len) {
    if (_ssid == "" || _ssid.length() > len)
        return false;
    strcpy(ssidbuf, _ssid.c_str());
    return true;
}

// 'len' stands for the max number of chars to copy excluding the nul byte 
bool WIFIConfig::get_wifi_passkey(char * keybuf, uint16_t len) {
    if (_pass == "" || _pass.length() > len)
        return false;
    strcpy(keybuf, _pass.c_str());
    return true;
}

String WIFIConfig::getConfigPortalSSID() {
  return _apName;
}

void WIFIConfig::resetSettings() {
  WC_DEBUG_PRINTLN("::settings invalidated");
  WC_DEBUG_PRINTLN("::THIS MAY CAUSE AP NOT TO START UP PROPERLY. YOU NEED TO COMMENT IT OUT AFTER ERASING THE DATA.");
  WiFi.disconnect(true);
  //delay(200);
}

void WIFIConfig::setConfigPortalTimeout(unsigned long seconds) {
  _configPortalTimeout = seconds * 1000UL;
}

/** Wifi config page handler */
void WIFIConfig::handleRoot(void) {
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Nodewire WiFi Config");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);

  page += FPSTR(HTTP_FORM_START);
  char parLength[5];
  // add the extra parameters to the form
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }

    String pitem = FPSTR(HTTP_FORM_PARAM);
    if (_params[i]->getID() != NULL) {
      pitem.replace("{i}", _params[i]->getID());
      pitem.replace("{n}", _params[i]->getID());
      pitem.replace("{p}", _params[i]->getPlaceholder());
      snprintf(parLength, 5, "%d", _params[i]->getValueLength());
      pitem.replace("{l}", parLength);
      pitem.replace("{v}", _params[i]->getValue());
      pitem.replace("{c}", _params[i]->getCustomHTML());
    } else {
      pitem = _params[i]->getCustomHTML();
    }

    page += pitem;
  }
  if (_params[0] != NULL) {
    page += "<br/>";
  }

  page += FPSTR(HTTP_FORM_END);
  page += FPSTR(HTTP_END);

  server->sendHeader("Content-Length", String(page.length()));
  server->send(200, "text/html", page);


  WC_DEBUG_PRINTLN("::Sent config page");
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WIFIConfig::handleWifiSave() {
  WC_DEBUG_PRINTLN("::WiFi save");

  //SAVE/connect here
  _ssid = server->arg("s").c_str();
  _pass = server->arg("p").c_str();

  //parameters
  for (int i = 0; i < _paramsCount; i++) {
    if (_params[i] == NULL) {
      break;
    }
    //read parameter
    String value = server->arg(_params[i]->getID()).c_str();
    //store it in array
    value.toCharArray(_params[i]->_value, _params[i]->_length);
    WC_DEBUG_PRINT("::Parameter: ");
    WC_DEBUG_PRINT(_params[i]->getID()); WC_DEBUG_PRINT(": ");
    WC_DEBUG_PRINTLN(value);
  }

  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Credentials Saved");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += FPSTR(HTTP_SAVED);
  page += FPSTR(HTTP_END);

  server->sendHeader("Content-Length", String(page.length()));
  server->send(200, "text/html", page);

  WC_DEBUG_PRINTLN("::Sent wifi save page");

  recvd_config = true; //signal ready to connect/reset
}

/** Handle the info page */
void WIFIConfig::handleInfo() {
  WC_DEBUG_PRINTLN("::Info");

  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("<dl>");
  page += F("<dt>Chip ID</dt><dd>");
  page += ESP.getChipId();
  page += F("</dd>");
  page += F("<dt>Flash Chip ID</dt><dd>");
  page += ESP.getFlashChipId();
  page += F("</dd>");
  page += F("<dt>IDE Flash Size</dt><dd>");
  page += ESP.getFlashChipSize();
  page += F(" bytes</dd>");
  page += F("<dt>Real Flash Size</dt><dd>");
  page += ESP.getFlashChipRealSize();
  page += F(" bytes</dd>");
  page += F("<dt>Soft AP IP</dt><dd>");
  page += WiFi.softAPIP().toString();
  page += F("</dd>");
  page += F("<dt>Soft AP MAC</dt><dd>");
  page += WiFi.softAPmacAddress();
  page += F("</dd>");
  page += F("<dt>Station MAC</dt><dd>");
  page += WiFi.macAddress();
  page += F("</dd>");
  page += F("</dl>");
  page += FPSTR(HTTP_END);

  server->sendHeader("Content-Length", String(page.length()));
  server->send(200, "text/html", page);

  WC_DEBUG_PRINTLN("Sent info page");
}

/** Handle the reset page */
void WIFIConfig::handleReset() {
  WC_DEBUG_PRINTLN("::Reset");
  
  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Info");
  page += FPSTR(HTTP_SCRIPT);
  page += FPSTR(HTTP_STYLE);
  page += _customHeadElement;
  page += FPSTR(HTTP_HEAD_END);
  page += F("Module will reset in a few seconds.");
  page += FPSTR(HTTP_END);

  server->sendHeader("Content-Length", String(page.length()));
  server->send(200, "text/html", page);

  WC_DEBUG_PRINTLN("::Sent reset page");
  delay(5000);
  ESP.restart();
  delay(2000);
}

void WIFIConfig::handleNotFound() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the error page.
    return;
  }
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += ( server->method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";

  for ( uint8_t i = 0; i < server->args(); i++ ) {
    message += " " + server->argName ( i ) + ": " + server->arg ( i ) + "\n";
  }
  server->sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server->sendHeader("Pragma", "no-cache");
  server->sendHeader("Expires", "-1");
  server->sendHeader("Content-Length", String(message.length()));
  server->send ( 404, "text/plain", message );
  delay(500);
  server->client().flush();
  server->client().stop(); // Stop is needed because we sent no content length
}


/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean WIFIConfig::captivePortal() {
  if (!isIp(server->hostHeader()) ) {
    WC_DEBUG_PRINTLN("::Request redirected to captive portal");
    String page = String("Go to ") + toStringIp(server->client().localIP());
    server->sendHeader("Location", String("http://") + toStringIp(server->client().localIP()), true);
    server->sendHeader("Content-Length", String(page.length()));
    server->send ( 302, "text/plain", page); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    delay(500);
    server->client().flush();
    server->client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

//start up save config callback
void WIFIConfig::setSaveConfigCallback( void (*func)(void) ) {
  _savecallback = func;
}

//sets a custom element to add to head, like a new style tag
void WIFIConfig::setCustomHeadElement(const char* element) {
  _customHeadElement = element;
}

/** Is this an IP? */
boolean WIFIConfig::isIp(String str) {
  for (int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String WIFIConfig::toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}
