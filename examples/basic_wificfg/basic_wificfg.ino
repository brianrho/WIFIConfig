#include <wificonfig.h>

/* max lengths of parameters to be obtained */
#define MAX_SSID_LEN        32
#define MAX_KEY_LEN         63
#define MAX_SERVER_LEN      30
#define MAX_UNAME_LEN       50
#define MAX_PWD_LEN         50

/* create the buffers to contain the results, also pass in any default value */
char ssid[MAX_SSID_LEN + 1];
char key[MAX_KEY_LEN + 1];
char uname[MAX_UNAME_LEN + 1];
char pwd[MAX_PWD_LEN + 1];
char server[MAX_SERVER_LEN + 1] = "defaultServer.com";

/* create the WIFIConfig instance with the page title */
WIFIConfig wcfg("The Portal");

/* Create custom parameters for username, password and server.
 *  Parameters are: a unique ID string, and a placeholder to be used in the web form,
 *  optionally custom HTML as the last argument.
 *  In this case, the custom HTML for the password parameter makes passwords appear 
 *  hidden when typed
 */
WIFIConfigParam uname_param("user", "Username");
WIFIConfigParam pwd_param("pwd", "Password", "type='password'");
WIFIConfigParam server_param("server", "Server (Optional)");

void setup() {
  Serial.begin(9600);
  Serial.println("WiFi Config test");

  /* set a timeout of 120 seconds, by default there is no timeout */
  wcfg.setConfigPortalTimeout(120);

  /* add the parameters created earlier.
   *  Pass a buffer to hold the parameters, 
   *  as well as the max number of bytes to be used in the buffer.
   *  If any specific parameter has a default value stored in its buffer,
   *  pass 'true' as the last argument.
   */
  wcfg.addParameter(&uname_param, uname, MAX_UNAME_LEN);
  wcfg.addParameter(&pwd_param, pwd, MAX_PWD_LEN);
  wcfg.addParameter(&server_param, server, MAX_SERVER_LEN, true);

  /* start the portal with the AP SSID and password */
  if (!wcfg.startConfigPortal("TestAP", "12345678")) {
    Serial.println("Failed to setup AP");
  }
  Serial.println("Soft AP started");
}

void loop() {
  /* constantly check the state of the config process */
  uint8_t ret = wcfg.config_loop();

  /* if it's completed, extract the results and print them, then stall */
  if (ret == WIFICONFIG_COMPLETE) {
    Serial.println("Config details: \r\n");
    
    if (wcfg.get_wifi_ssid(ssid, MAX_SSID_LEN))
      Serial.printf("SSID: %s\r\n", ssid);
    if (wcfg.get_wifi_passkey(key, MAX_KEY_LEN))
      Serial.printf("Passkey: %s\r\n", key);
      
    Serial.printf("Username: %s\r\n", uname);
    Serial.printf("Password: %s\r\n", pwd);
    Serial.printf("Server: %s\r\n", server);
    while (1) yield();
  }
  /* if it timed out, simply stall forever */
  else if (ret == WIFICONFIG_TIMEOUT) {
    Serial.println("Config timed out.");
    while (1) yield();
  }
}