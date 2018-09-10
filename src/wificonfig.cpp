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

const char WC_HTTP_HEAD[] PROGMEM            = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
const char WC_HTTP_STYLE[] PROGMEM           = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}</style>";
const char WC_HTTP_SCRIPT[] PROGMEM          = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char WC_HTTP_HEAD_END[] PROGMEM        = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char WC_HTTP_ITEM[] PROGMEM            = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char WC_HTTP_FORM_START[] PROGMEM      = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='SSID'><br/><input id='p' name='p' length=64 type='password' placeholder='Passkey'><br/>";
const char WC_HTTP_FORM_PARAM[] PROGMEM      = "<br/><input id='{i}' name='{n}' maxlength={l} placeholder='{p}' value='{v}' {c}>";
const char WC_HTTP_FORM_END[] PROGMEM        = "<br/><button type='submit'>Configure</button></form>";
const char WC_HTTP_SAVED[] PROGMEM           = "<div>Configuration successful.<br /></div>";
const char WC_HTTP_END[] PROGMEM             = "</div></body></html>";

WIFIConfigParam::WIFIConfigParam(const char *custom) {
    _id = NULL;
    _placeholder = NULL;
    _length = 0;
    _value = NULL;

    _customHTML = custom;
}

// Length parameter indicates max length of string to copy, excluding terminating nul
WIFIConfigParam::WIFIConfigParam(const char *id, const char *placeholder) {
    init(id, placeholder, "");
}

WIFIConfigParam::WIFIConfigParam(const char *id, const char *placeholder, const char *custom) {
    init(id, placeholder, custom);
}

void WIFIConfigParam::init(const char *id, const char *placeholder, const char *custom) {
    _id = id;
    _placeholder = placeholder;

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

WIFIConfig::WIFIConfig(const char * page_title) : 
server(80), _page_title(page_title)
{
    
}

void WIFIConfig::addParameter(WIFIConfigParam *p, char *buffer, int length, bool hasDefault) {
    if(_paramsCount + 1 > WIFICONFIG_MAX_PARAMS)
    {
        //Max parameters exceeded!
        WC_DEBUG_PRINTLN("WIFICONFIG_MAX_PARAMS exceeded, increase number (in WIFIConfig.h) before adding more parameters!");
        WC_DEBUG_PRINTLN("Skipping parameter with ID:");
        WC_DEBUG_PRINTLN(p->getID());
        return;
    }

    p->_value = buffer;
    p->_length = length;

    // nul terminate the parameter buffer, if there's no default value
    if (!hasDefault && p->_value != NULL && p->_length != 0)
    p->_value[0] = 0;

    _params[_paramsCount] = p;
    _paramsCount++;
    WC_DEBUG_PRINT("::Adding parameter: ");
    WC_DEBUG_PRINTLN(p->getID());
}

void WIFIConfig::resetParameterList(void) {
    _paramsCount = 0;
}

void WIFIConfig::setupConfigPortal() {
    dnsServer.reset(new DNSServer());

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
    server.on("/", std::bind(&WIFIConfig::handleRoot, this, std::placeholders::_1));
    server.on("/wifisave", std::bind(&WIFIConfig::handleWifiSave, this, std::placeholders::_1));
    server.on("/i", std::bind(&WIFIConfig::handleInfo, this, std::placeholders::_1));
    server.on("/r", std::bind(&WIFIConfig::handleReset, this, std::placeholders::_1));
    server.on("/fwlink", std::bind(&WIFIConfig::handleRoot, this, std::placeholders::_1));  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
    server.onNotFound (std::bind(&WIFIConfig::handleRoot, this, std::placeholders::_1));
    server.begin(); // Web server start
    WC_DEBUG_PRINTLN("::HTTP server started");

}

boolean WIFIConfig::configPortalHasTimeout() {
    if(_configPortalTimeout == 0 || WiFi.softAPgetStationNum() > 0) {
        _configPortalStart = millis(); // kludge, bump configportal start time to skew timeouts
        return false;
    }
    return (millis() > _configPortalStart + _configPortalTimeout);
}

boolean WIFIConfig::startConfigPortal() {
#if defined(ARDUINO_ARCH_ESP8266)
    String ssid = "ESP" + String(ESP.getChipId());
#elif defined(ARDUINO_ARCH_ESP32)
    uint64_t chip_id = ESP.getEfuseMac();
    String ssid = "ESP" + String((uint16_t)(chip_id >> 32));
    ssid += String((uint32_t)(chip_id & 0x0000FFFFFFFF));
#endif
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
        WC_DEBUG_PRINTLN("::Timeout. Stopping servers");
        cleanup();
    }
    
    else if (recvd_config) {
        recvd_config = false;
        delay(1000);
        WC_DEBUG_PRINTLN("::Done. Stopping servers");
        
        cleanup();
        config_state = WIFICONFIG_COMPLETE;
        
        if ( _savecallback != NULL) {
            _savecallback();
        }
    }
    else {
        dnsServer->processNextRequest();
    }
    
    return config_state;
}

// 'len' stands for the max number of chars to copy excluding the nul byte 
bool WIFIConfig::get_wifi_ssid(char * ssidbuf, uint16_t len) {
    if (_ssid == "" || _ssid.length() > len)
    return false;
    _ssid.toCharArray(ssidbuf, len + 1);
    return true;
}

// 'len' stands for the max number of chars to copy excluding the nul byte 
bool WIFIConfig::get_wifi_passkey(char * keybuf, uint16_t len) {
    if (_pass == "" || _pass.length() > len)
    return false;
    _pass.toCharArray(keybuf, len + 1);
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
void WIFIConfig::handleRoot(AsyncWebServerRequest * request) {
    String page = FPSTR(WC_HTTP_HEAD);
    if (_page_title != NULL)
        page.replace("{v}", _page_title);
    else
        page.replace("{v}", "WiFi Config Portal");

    page += FPSTR(WC_HTTP_SCRIPT);
    page += FPSTR(WC_HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(WC_HTTP_HEAD_END);

    page += FPSTR(WC_HTTP_FORM_START);
    char parLength[5];
    // add the extra parameters to the form
    for (int i = 0; i < _paramsCount; i++) {
        if (_params[i] == NULL) {
            break;
        }

        String pitem = FPSTR(WC_HTTP_FORM_PARAM);
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

    page += FPSTR(WC_HTTP_FORM_END);
    page += FPSTR(WC_HTTP_END);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", page);
    response->addHeader("Content-Length", String(page.length()));
    request->send(response);

    WC_DEBUG_PRINTLN("::Sent config page");
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void WIFIConfig::handleWifiSave(AsyncWebServerRequest * request) {
    WC_DEBUG_PRINTLN("::WiFi save");

    //SAVE/connect here
    _ssid = request->arg("s").c_str();
    _pass = request->arg("p").c_str();

    //parameters
    for (int i = 0; i < _paramsCount; i++) {
        if (_params[i] == NULL) {
            break;
        }
        //read parameter
        String value = request->arg(_params[i]->getID()).c_str();
        //store it in array
        value.toCharArray(_params[i]->_value, _params[i]->_length + 1);
        WC_DEBUG_PRINT("::Parameter: ");
        WC_DEBUG_PRINT(_params[i]->getID()); WC_DEBUG_PRINT(": ");
        WC_DEBUG_PRINTLN(value);
    }

    String page = FPSTR(WC_HTTP_HEAD);
    page.replace("{v}", "Credentials Saved");
    page += FPSTR(WC_HTTP_SCRIPT);
    page += FPSTR(WC_HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(WC_HTTP_HEAD_END);
    page += FPSTR(WC_HTTP_SAVED);
    page += FPSTR(WC_HTTP_END);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", page);
    response->addHeader("Content-Length", String(page.length()));
    request->send(response);

    WC_DEBUG_PRINTLN("::Sent wifi save page");

    recvd_config = true; //signal ready to connect/reset
}

/** Handle the info page */
void WIFIConfig::handleInfo(AsyncWebServerRequest * request) {
    WC_DEBUG_PRINTLN("::Info");

    String page = FPSTR(WC_HTTP_HEAD);
    page.replace("{v}", "Info");
    page += FPSTR(WC_HTTP_SCRIPT);
    page += FPSTR(WC_HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(WC_HTTP_HEAD_END);
    page += F("<dl>");
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
    page += FPSTR(WC_HTTP_END);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", page);
    response->addHeader("Content-Length", String(page.length()));
    request->send(response);

    WC_DEBUG_PRINTLN("Sent info page");
}

/** Handle the reset page */
void WIFIConfig::handleReset(AsyncWebServerRequest * request) {
    WC_DEBUG_PRINTLN("::Reset");

    String page = FPSTR(WC_HTTP_HEAD);
    page.replace("{v}", "Info");
    page += FPSTR(WC_HTTP_SCRIPT);
    page += FPSTR(WC_HTTP_STYLE);
    page += _customHeadElement;
    page += FPSTR(WC_HTTP_HEAD_END);
    page += F("Module will reset in a few seconds.");
    page += FPSTR(WC_HTTP_END);

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", page);
    response->addHeader("Content-Length", String(page.length()));
    request->send(response);

    WC_DEBUG_PRINTLN("::Sent reset page");
    delay(5000);
    ESP.restart();
    delay(2000);
}

void WIFIConfig::handleNotFound(AsyncWebServerRequest * request) {
    request->redirect("/");
}

//start up save config callback
void WIFIConfig::setSaveConfigCallback( void (*func)(void) ) {
    _savecallback = func;
}

//sets a custom element to add to head, like a new style tag
void WIFIConfig::setCustomHeadElement(const char* element) {
    _customHeadElement = element;
}

void WIFIConfig::cleanup(void) {
    dnsServer->stop();
    dnsServer.reset();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}
