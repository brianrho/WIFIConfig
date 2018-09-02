/**************************************************************
   WIFIConfig is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer/examples/CaptivePortalAdvanced
   Built by AlexT https://github.com/tzapu
   Licensed under MIT license
 **************************************************************/

#ifndef WIFIConfig_h
#define WIFIConfig_h

#if defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
#elif defined(ARDUINO_ARCH_ESP32)
    #include <WiFi.h>
    #include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <memory>

//#define WC_ENABLE_DEBUG

#if defined(WC_ENABLE_DEBUG)
    #define WC_DEFAULT_STREAM          Serial

    #define WC_DEBUG_PRINT(x)          WC_DEFAULT_STREAM.print(x)
    #define WC_DEBUG_PRINTLN(x)        WC_DEFAULT_STREAM.println(x)
    #define WC_DEBUG_DEC(x)            WC_DEFAULT_STREAM.print(x)
    #define WC_DEBUG_DECLN(x)          WC_DEFAULT_STREAM.println(x)
    #define WC_DEBUG_HEX(x)            WC_DEFAULT_STREAM.print(x, HEX)
    #define WC_DEBUG_HEXLN(x)          WC_DEFAULT_STREAM.println(x, HEX) 
#else
    #define WC_DEBUG_PRINT(x)
    #define WC_DEBUG_PRINTLN(x)
    #define WC_DEBUG_DEC(x)          
    #define WC_DEBUG_DECLN(x)
    #define WC_DEBUG_HEX(x)
    #define WC_DEBUG_HEXLN(x)
#endif

#define WIFICONFIG_MAX_PARAMS 10

enum {
    WIFICONFIG_COMPLETE,
    WIFICONFIG_NOTSTARTED,
    WIFICONFIG_INPROGRESS,
    WIFICONFIG_TIMEOUT,
};

class WIFIConfigParam {
  public:
    WIFIConfigParam(const char *custom);
    WIFIConfigParam(const char *id, const char *placeholder);
    WIFIConfigParam(const char *id, const char *placeholder, const char *custom);

    const char *getID();
    const char *getValue();
    const char *getPlaceholder();
    int         getValueLength();
    const char *getCustomHTML();
  private:
    const char *_id;
    const char *_placeholder;
    char       *_value;
    int         _length;
    const char *_customHTML;

    void init(const char *id, const char *placeholder, const char *custom);

    friend class WIFIConfig;
};


class WIFIConfig
{
  public:
    WIFIConfig();

    //if you want to always start the config portal, without trying to connect first
    boolean       startConfigPortal();
    boolean       startConfigPortal(char const *apName, char const *apPassword = NULL);

    // get the AP name of the config portal, so it can be used in the callback
    String        getConfigPortalSSID();

    void          resetSettings();

    //sets timeout before webserver loop ends and exits even if there has been no setup.
    //useful for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout
    void          setConfigPortalTimeout(unsigned long seconds);
    //called when settings have been changed and connection was successful
    void          setSaveConfigCallback( void (*func)(void) );
    //adds a custom parameter, additional parameters include a buffer for storing the parameter,
    // its length and if there's a default value stored within the buffer
    void          addParameter(WIFIConfigParam *p, char *buffer, int length, bool hasDefault = false);
    /* this resets the parameter list to zero so you can re-add parameters */
    void          resetParameterList(void);
    //TODO
    //if this is set, customise style
    void          setCustomHeadElement(const char* element);

    // get the WIFI ssid and key after configuration,
    // pass in a buffer and the maximum number of characters to copy
    bool          get_wifi_ssid(char * ssidbuf, uint16_t len);
    bool          get_wifi_passkey(char * keybuf, uint16_t len);
    uint8_t       config_loop(void);
  private:
    std::unique_ptr<DNSServer>        dnsServer;
    
    /* AsyncWebServer causes an exception when deleted,
     * there's no stop() or close(), reset() frees everything but 
     * there's no easy way to restart the server if it's needed short of creating a new instance,
     * and if you do that with startConfigPortal(), the old one's still hanging around, un-deletable and leaking memory,
     * so we'll just have to live with a member variable for now if we ever want to reuse the server */
    AsyncWebServer server;
    
    void          setupConfigPortal();

    const char*   _apName                 = "no-net";
    const char*   _apPassword             = NULL;
    String        _ssid                   = "";
    String        _pass                   = "";
    unsigned long _configPortalTimeout    = 0;
    unsigned long _configPortalStart      = 0;

    int           _paramsCount            = 0;

    const char*   _customHeadElement      = "";
    
    void          handleRoot(AsyncWebServerRequest * request);
    void          handleWifiSave(AsyncWebServerRequest * request);
    void          handleInfo(AsyncWebServerRequest * request);
    void          handleReset(AsyncWebServerRequest * request);
    void          handleNotFound(AsyncWebServerRequest * request);
    boolean       configPortalHasTimeout();
    void          cleanup(void);

    // DNS server
    const byte    DNS_PORT = 53;

    boolean       recvd_config = false;
    uint8_t       config_state = WIFICONFIG_NOTSTARTED;
    
    void (*_savecallback)(void) = NULL;

    WIFIConfigParam* _params[WIFICONFIG_MAX_PARAMS];
};

#endif
