#include <wificonfig.h>

#define MAX_SSID_LEN        32
#define MAX_KEY_LEN         63
#define MAX_SERVER_LEN      30
#define MAX_UNAME_LEN       50
#define MAX_PWD_LEN         50

char ssid[MAX_SSID_LEN + 1];
char key[MAX_KEY_LEN + 1];
char uname[MAX_UNAME_LEN + 1];
char pwd[MAX_PWD_LEN + 1];
char server[MAX_SERVER_LEN + 1] = "defaultServer.com";

WIFIConfig wcfg;
WIFIConfigParam uname_param("user", "Username", uname, MAX_UNAME_LEN);
WIFIConfigParam pwd_param("pwd", "Password", pwd, MAX_PWD_LEN, "type='password'");
WIFIConfigParam server_param("server", "Server (Optional)", server, MAX_SERVER_LEN);

void setup() {
  Serial.begin(9600);
  Serial.println("WiFi Config test");
  wcfg.setConfigPortalTimeout(120);
  wcfg.addParameter(&uname_param);
  wcfg.addParameter(&pwd_param);
  wcfg.addParameter(&server_param, true);
  
  if (!wcfg.startConfigPortal("TestAP", "12345678")) {
    Serial.println("Failed to setup AP");
  }
  Serial.println("Soft AP started");
}

void loop() {
  uint8_t ret = wcfg.config_loop();
  
  if (ret == WIFICONFIG_COMPLETE) {
    Serial.println("Config details: \r\n");
    
    if (wcfg.get_wifi_ssid(ssid, MAX_SSID_LEN))
      Serial.printf("SSID: %s\r\n", ssid);
    if (wcfg.get_wifi_passkey(key, MAX_KEY_LEN))
      Serial.printf("Passkey: %s\r\n", key);
    strcpy(uname, uname_param.getValue());
    Serial.printf("Server: %s\r\n", uname);
    strcpy(pwd, pwd_param.getValue());
    Serial.printf("Password: %s\r\n", pwd);
    strcpy(server, server_param.getValue());
    Serial.printf("Server: %s\r\n", server);
    while (1) yield();
  }
  else if (ret == WIFICONFIG_TIMEOUT) {
    Serial.println("Config timed out.");
    while (1) yield();
  }
}