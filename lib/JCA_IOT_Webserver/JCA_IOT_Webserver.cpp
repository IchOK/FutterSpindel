/**
 * @file JCA_IOT_Webserver.cpp
 * @author JCA (https://github.com/ichok)
 * @brief The Class contains a Webserver (with WebSocket)
 * It contains the folloing Modules
 * - Navigation
 *   - Home [/home.htm], to be define by User (Startpage)
 *   - Config [/config.htm], to be define by User (Application Config Informations)
 *   - Connect [/connect -> PageFrame + SectionConnect], WiFi Connection Settings.
 *   - System [/sys -> PageFrame + SectionSys]
 *     - Download App config [config.json]
 *     - Upload Web-Content [*.json, *.htm, *.html, *.js, *.css]
 *     - Firmware Update [*.bin]
 *     - Reset the controller
 * - Style Sheet
 * - Navigation and Logo Icons
 * @version 0.1
 * @date 2022-09-04
 *
 * Copyright Jochen Cabrera 2022
 * Apache License
 *
 */
#include <JCA_IOT_Webserver.h>

namespace JCA {
  namespace IOT {
    /**
     * @brief Construct a new Webserver::Webserver object
     *
     * @param _HostnamePrefix Prefix for default Hostname ist not defined in Config
     * @param _Port Port of the Webserver if not defined in Config
     * @param _ConfUser Username for the System Sites
     * @param _ConfPassword Password for the System Sites
     */
    Webserver::Webserver (const char *_HostnamePrefix, uint16_t _Port, const char *_ConfUser, const char *_ConfPassword) : Server (_Port), Websocket ("/ws") {
      sprintf (Hostname, "%s_%08X", _HostnamePrefix, ESP.getChipId ());
      Port = _Port;
      Reboot = false;
      strncpy (ConfUser, _ConfUser, sizeof (ConfUser));
      strncpy (ConfPassword, _ConfPassword, sizeof (ConfPassword));
    }

    /**
     * @brief Construct a new Webserver::Webserver object
     * Username and Password for System Sites is set to Default
     * @param _HostnamePrefix Prefix for default Hostname ist not defined in Config
     * @param _Port Port of the Webserver if not defined in Config
     */
    Webserver::Webserver (const char *_HostnamePrefix, uint16_t _Port) : Webserver (_HostnamePrefix, _Port, JCA_IOT_WEBSERVER_DEFAULT_CONF_USER, JCA_IOT_WEBSERVER_DEFAULT_CONF_PASS) {
    }

    /**
     * @brief Construct a new Webserver::Webserver object
     * Username and Password for System Sites is set to Default
     * Hostname-Prefix and Webserver-Port ist set to Default
     */
    Webserver::Webserver () : Webserver (JCA_IOT_WEBSERVER_DEFAULT_HOSTNAMEPREFIX, JCA_IOT_WEBSERVER_DEFAULT_PORT) {
    }

    /**
     * @brief Loop Function to handle Connection State
     *
     * @return true Controller is connected to WiFI
     * @return false Controller runs as AP
     */
    bool Webserver::handle () {
      Connector.handle ();
      return Connector.isConnected ();
    }

    /**
     * @brief Define all Default Web-Requests and init the Webserver
     *
     * @return true Controller is connected to WiFI
     * @return false Controller runs as AP
     */
    bool Webserver::init () {
      // WiFi Connection - Init First
      Connector.init ();

      // WebSocket - Init
      // Server.addHandle(&Websocket);

      // Webserver - WiFi Config
      Server.on (JCA_IOT_WEBSERVER_PATH_CONNECT, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onConnectGet (_Request); });
      Server.on (JCA_IOT_WEBSERVER_PATH_CONNECT, HTTP_POST, [this] (AsyncWebServerRequest *_Request) { this->onConnectPost (_Request); });
      if (Connector.isConnected ()) {
        Server.on ("/", HTTP_GET, [] (AsyncWebServerRequest *request) { request->redirect (JCA_IOT_WEBSERVER_PATH_HOME); });
      } else {
        Server.on ("/", HTTP_GET, [] (AsyncWebServerRequest *request) { request->redirect (JCA_IOT_WEBSERVER_PATH_CONNECT); });
      }

      // Webserver - System Config
      Server.on (JCA_IOT_WEBSERVER_PATH_SYS, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onSystemGet (_Request); });
      Server.on (
          JCA_IOT_WEBSERVER_PATH_SYS_UPLOAD, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { _Request->redirect (JCA_IOT_WEBSERVER_PATH_SYS); },
          [this] (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) { this->onSystemUploadData (_Request, _Filename, _Index, _Data, _Len, _Final); });
      Server.on (
          JCA_IOT_WEBSERVER_PATH_SYS_UPDATE, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onSystemUpdate (_Request); },
          [this] (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) { this->onSystemUpdateData (_Request, _Filename, _Index, _Data, _Len, _Final); });
      Server.on (JCA_IOT_WEBSERVER_PATH_SYS_RESET, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onSystemReset (_Request); });

      // Webserver - Custom Pages
      Server.on (JCA_IOT_WEBSERVER_PATH_HOME, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onHomeGet (_Request); });
      Server.on (JCA_IOT_WEBSERVER_PATH_CONFIG, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onConfigGet (_Request); });

      // Webserver - If not defined
      Server.serveStatic ("/", LittleFS, "/").setDefaultFile (JCA_IOT_WEBSERVER_PATH_HOME);
      Server.onNotFound ([] (AsyncWebServerRequest *_Request) { _Request->send (404); });
      Server.begin ();

      return Connector.isConnected ();
    }
    
    /**
     * @brief Read the Config-JSON and store the Data
     * 
     * @return true Config-File exists and is valid
     * @return false Config-File didn't exists or is not a valid JSON File
     */
    bool Webserver::readConfig () {
      const char *FunctionName = "readConfig";
      // Get Wifi-Config from File5
      File ConfigFile = LittleFS.open (JCA_IOT_WEBSERVER_CONFIGPATH, "r");
      if (ConfigFile) {
        Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config File Found");
        DeserializationError Error = deserializeJson (JsonDoc, ConfigFile);
        if (!Error) {
          Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Deserialize Done");
          JsonObject Config = JsonDoc.as<JsonObject> ();
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config contains WiFi");
            JsonObject WiFiConfig = Config[JCA_IOT_WEBSERVER_CONFKEY_WIFI].as<JsonObject> ();
            // Get Network Config
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_SSID)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found SSID");
              if (!Connector.setSsid (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_SSID].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] SSID invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_PASS)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found Password");
              if (!Connector.setPassword (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_PASS].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] Password invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_IP)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found IP");
              if (!Connector.setIP (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_IP].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] IP invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_GATEWAY)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found Gateway");
              if (!Connector.setGateway (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_GATEWAY].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] Gateway invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_SUBNET)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found Subnet");
              if (!Connector.setSubnet (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_SUBNET].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] Subnet invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_DHCP)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "[WiFi] Found DHCP");
              if (!Connector.setDHCP (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_DHCP].as<bool> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "[WiFi] DHCP invalid");
              }
            }
          }

          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_HOSTNAME)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config contains Hostname");
            strncpy (Hostname, Config[JCA_IOT_WEBSERVER_CONFKEY_HOSTNAME].as<const char *> (), sizeof (Hostname));
          }

          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_PORT)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config contains Serverport");
            Port = Config[JCA_IOT_WEBSERVER_CONFKEY_PORT].as<uint16_t> ();
          }
        } else {
          Debug.print (FLAG_ERROR, true, ObjectName, FunctionName, "deserializeJson() failed: ");
          Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, Error.c_str ());
          return false;
        }
        ConfigFile.close ();
      } else {
        Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "Config File NOT found");
        return false;
      }
      return true;
    }

    /**
     * @brief Handle the POST-Request on the Connection-Site
     * 
     * @param _Request Request data from Web-Client
     */
    void Webserver::onConnectPost (AsyncWebServerRequest *_Request) {
      const char *FunctionName = "onConnectPost";
      JsonObject Config;
      JsonObject WiFiConfig;

      File ConfigFile = LittleFS.open (JCA_IOT_WEBSERVER_CONFIGPATH, "r");
      if (ConfigFile) {
        Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config File Found");
        DeserializationError Error = deserializeJson (JsonDoc, ConfigFile);
        if (!Error) {
          Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Deserialize Done");
          Config = JsonDoc.as<JsonObject> ();
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config Node Found");
            WiFiConfig = Config[JCA_IOT_WEBSERVER_CONFKEY_WIFI].as<JsonObject> ();
          } else {
            Debug.println (FLAG_CONFIG, true, ObjectName, FunctionName, "Config Node Created");
            WiFiConfig = Config.createNestedObject (JCA_IOT_WEBSERVER_CONFKEY_WIFI);
          }
        } else {
          Debug.print (FLAG_ERROR, true, ObjectName, FunctionName, "deserializeJson() failed: ");
          Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, Error.c_str ());
          Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "Create new Konfig");
          JsonDoc.clear ();
          Config = JsonDoc.as<JsonObject> ();
          WiFiConfig = Config.createNestedObject (JCA_IOT_WEBSERVER_CONFKEY_WIFI);
        }
      } else {
        Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "Config File NOT found");
        Debug.println (FLAG_ERROR, true, ObjectName, FunctionName, "Create new Konfig");
        JsonDoc.clear ();
        Config = JsonDoc.as<JsonObject> ();
        WiFiConfig = Config.createNestedObject (JCA_IOT_WEBSERVER_CONFKEY_WIFI);
      }

      int params = _Request->params ();
      for (int i = 0; i < params; i++) {
        AsyncWebParameter *p = _Request->getParam (i);
        if (p->isPost ()) {
          if (p->name () == JCA_IOT_WEBSERVER_CONFKEY_WIFI_DHCP) {
            if (p->value () == "on") {
              WiFiConfig[p->name ()] = true;
            } else {
              WiFiConfig[p->name ()] = false;
            }
          } else {
            WiFiConfig[p->name ()] = p->value ().c_str ();
          }
        }
      }

      // Answer to the Client
      if (Connector.doConnect ()) {
        _Request->send (200, "text/plain", "Connect to Network");
      } else {
        _Request->send (200, "text/plain", "Network Config invalid");
      }
    }

    /**
     * @brief Handle the GET-Request from the Connection-Site
     *
     * @param _Request Request data from Web-Client
     */
    void Webserver::onConnectGet (AsyncWebServerRequest *_Request) {
      if (!_Request->authenticate (ConfUser, ConfPassword)) {
        return _Request->requestAuthentication ();
      }
      _Request->send_P (200, "text/html", PageFrame, [this] (const String &_Var) -> String { return this->replaceConnectWildcards (_Var); });
    }

    /**
     * @brief Handle the GET-Request for the System-Site
     *
     * @param _Request Request data from Web-Client
     */
    void Webserver::onSystemGet (AsyncWebServerRequest *_Request) {
      if (!_Request->authenticate (ConfUser, ConfPassword)) {
        return _Request->requestAuthentication ();
      }
      _Request->send_P (200, "text/html", PageFrame, [this] (const String &_Var) -> String { return this->replaceSystemWildcards (_Var); });
    }

    /**
     * @brief Handle the File-Upload on the System-Site
     *
     * @param _Request Request data from Web-Client
     * @param _Filename Name of uploaded File
     * @param _Index Offset of the Data from the Datablock
     * @param _Data File data
     * @param _Len Length of the Datablock
     * @param _Final Last Datablock of the uploaded File
     */
    void Webserver::onSystemUploadData (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) {
      const char *FunctionName = "onSystemUploadData";
      if (!_Request->authenticate (ConfUser, ConfPassword)) {
        return _Request->requestAuthentication ();
      }
      Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, String ("Client:" + _Request->client ()->remoteIP ().toString () + " " + _Request->url ()));
      if (!_Index) {
        Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, String ("Upload Start: " + String (_Filename)));
        // open the file on first call and store the file handle in the request object
        _Request->_tempFile = LittleFS.open ("/" + _Filename, "w");
      }
      if (_Len) {
        Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, String ("Writing file: " + String (_Filename) + " index=" + String (_Index) + " len=" + String (_Len)));
        // stream the incoming chunk to the opened file
        _Request->_tempFile.write (_Data, _Len);
      }
      if (_Final) {
        Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, String ("Upload Complete: " + String (_Filename) + ",size: " + String (_Index + _Len)));
        // close the file handle as the upload is now done
        _Request->_tempFile.close ();
      }
    }

    /**
     * @brief Handle the Response to the Clinte after Firmware Update
     *
     * @param _Request Request data from Web-Client
     */
    void Webserver::onSystemUpdate (AsyncWebServerRequest *_Request) {
      Reboot = !Update.hasError ();
      AsyncWebServerResponse *Response = _Request->beginResponse (301);
      Response->addHeader ("Location", JCA_IOT_WEBSERVER_PATH_SYS);
      Response->addHeader ("Retry-After", "60");
      _Request->send (Response);
    }

    /**
     * @brief Handle the Firmare-Update on the System-Site
     *
     * @param _Request Request data from Web-Client
     * @param _Filename Name of Firmware File
     * @param _Index Offset of the Data from the Datablock
     * @param _Data File data
     * @param _Len Length of the Datablock
     * @param _Final Last Datablock of the Firmware File
     */
    void Webserver::onSystemUpdateData (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) {
      const char *FunctionName = "onSystemUpdateData";
      if (!_Request->authenticate (ConfUser, ConfPassword)) {
        return _Request->requestAuthentication ();
      }
      if (!_Index) {
        Debug.print (FLAG_TRAFFIC, true, ObjectName, FunctionName, "Update Start: ");
        Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, _Filename.c_str ());
        Update.runAsync (true);
        if (!Update.begin ((ESP.getFreeSketchSpace () - 0x1000) & 0xFFFFF000)) {
          if (Debug.print (FLAG_ERROR, true, ObjectName, FunctionName, "")){
            Update.printError (Serial);
          }
        }
      }
      if (!Update.hasError ()) {
        if (Update.write (_Data, _Len) != _Len) {
          if (Debug.print (FLAG_ERROR, true, ObjectName, FunctionName, "")) {
            Update.printError (Serial);
          }
        }
      }
      if (_Final) {
        if (Update.end (true)) {
          Debug.print (FLAG_TRAFFIC, true, ObjectName, FunctionName, "Update Success: ");
          Debug.println (FLAG_TRAFFIC, true, ObjectName, FunctionName, _Index + _Len);
        } else {
          if (Debug.print (FLAG_ERROR, true, ObjectName, FunctionName, "")) {
            Update.printError (Serial);
          }
        }
      }
    }

    /**
     * @brief Initialize the Controller Reset
     *
     * @param _Request Request data from Web-Client
     */
    void Webserver::onSystemReset (AsyncWebServerRequest *_Request) {
      if (!_Request->authenticate (ConfUser, ConfPassword)) {
        return _Request->requestAuthentication ();
      }
      Reboot = true;
      AsyncWebServerResponse *Response = _Request->beginResponse (301);
      Response->addHeader ("Location", JCA_IOT_WEBSERVER_PATH_SYS);
      Response->addHeader ("Retry-After", "60");
      _Request->send (Response);
    }

    void Webserver::onHomeGet (AsyncWebServerRequest *_Request) {
      _Request->send (LittleFS, JCA_IOT_WEBSERVER_PATH_HOME, String (), false, [this] (const String &_Var) -> String { return this->replaceHomeWildcards (_Var); });
    }

    void Webserver::onConfigGet (AsyncWebServerRequest *_Request) {
      _Request->send (LittleFS, JCA_IOT_WEBSERVER_PATH_CONFIG, String (), false, [this] (const String &_Var) -> String { return this->replaceConfigWildcards (_Var); });
    }

    /**
     * @brief Replace Default Wildcards in Websites
     * 
     * @param var Wildcard
     * @return String Replace String
     */
    String Webserver::replaceDefaultWildcards (const String &var) {
      if (var == "TITLE") {
        return String (Hostname);
      }
      if (var == "SVG_LOGO") {
        return String (SvgLogo);
      }
      if (var == "SVG_HOME") {
        return String (SvgHome);
      }
      if (var == "SVG_CONFIG") {
        return String (SvgConfig);
      }
      if (var == "SVG_WIFI") {
        return String (SvgWiFi);
      }
      if (var == "SVG_SYSTEM") {
        return String (SvgSystem);
      }
      return String ();
    }

    /**
     * @brief Replace Wildcards of Home Site using the external Replace Callback Function
     *
     * @param var Wildcard
     * @return String Replace String
     */
    String Webserver::replaceHomeWildcards (const String &var) {
      String RetVal;
      RetVal = (replaceConfigWildcardsCB)(var);
      if (RetVal) {
        return RetVal;
      }
      RetVal = replaceDefaultWildcards (var);
      if (RetVal) {
        return RetVal;
      }
      return String ();
    }

    /**
     * @brief Replace Wildcards of Config Site using the external Replace Callback Function
     *
     * @param var Wildcard
     * @return String Replace String
     */
    String Webserver::replaceConfigWildcards (const String &var) {
      String RetVal;
      RetVal = (replaceConfigWildcardsCB)(var);
      if (RetVal) {
        return RetVal;
      }
      RetVal = replaceDefaultWildcards (var);
      if (RetVal) {
        return RetVal;
      }
      return String ();
    }

    /**
     * @brief Replace Wildcards of System Site
     *
     * @param var Wildcard
     * @return String Replace String
     */
    String Webserver::replaceSystemWildcards (const String &var) {
      String RetVal;
      RetVal = replaceDefaultWildcards (var);
      if (RetVal) {
        return RetVal;
      }
      if (var == "NAME") {
        return F ("System");
      }
      if (var == "STYLE") {
        return F (":root{--ColorSystem:var(--contrast)}");
      }
      if (var == "SECTION") {
        return String (SectionSys);
      }
      if (var == "FW_VERSION") {
        return String (FirmwareVersion);
      }
      if (var == "FW_BUILD") {
        return String (FirmwareBuild);
      }
      if (var == "BOARD_NAME") {
        return String (ARDUINO_BOARD);
      }
      if (var == "BOARD_VERSION") {
        return String (ARDUINO_ESP8266_RELEASE);
      }
      if (var == "BOARD_VARIANT") {
        return String (BOARD_VARIANT);
      }
      if (var == "BOARD_MCU") {
        return String (BOARD_MCU);
      }
      return String ();
    }

    /**
     * @brief Replace Wildcards of Connect Site
     *
     * @param var Wildcard
     * @return String Replace String
     */
    String Webserver::replaceConnectWildcards (const String &var) {
      String RetVal;
      RetVal = Connector.replaceWildcards (var);
      if (RetVal) {
        return RetVal;
      }
      RetVal = replaceDefaultWildcards (var);
      if (RetVal) {
        return RetVal;
      }
      if (var == "SECTION") {
        return String (SectionConnect);
      }
      return String ();
    }
  }
}