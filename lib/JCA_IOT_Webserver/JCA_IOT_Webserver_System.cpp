/**
 * @file JCA_IOT_Webserver_System.cpp
 * @author JCA (https://github.com/ichok)
 * @brief System-Functions of the Webserver
 * @version 0.1
 * @date 2022-09-07
 *
 * Copyright Jochen Cabrera 2022
 * Apache License
 *
 */
#include <JCA_IOT_Webserver.h>
using namespace JCA::SYS;
using namespace JCA::FNC;

namespace JCA {
  namespace IOT {
    const char *Webserver::ElementName = "System";
    const char *Webserver::Hostname_Name = "hostname";
    const char *Webserver::Hostname_Text = "Hostname";
    const char *Webserver::Hostname_Comment = "Hostname wirde erst nache dem Reboot aktiv";
    const char *Webserver::WsUpdateCycle_Name = "wsUpdate";
    const char *Webserver::WsUpdateCycle_Text  = "Websocket Updatezyklus";
    const char *Webserver::WsUpdateCycle_Unit = "ms";
    const char *Webserver::WsUpdateCycle_Comment = nullptr;
    const char *Webserver::TimeSync_Name = "timeSync";
    const char *Webserver::TimeSync_Text = "Uhrzeit syncronisieren";
    const char *Webserver::TimeSync_Type = "uint32";
    const char *Webserver::TimeSync_Comment = nullptr;
    const char *Webserver::SaveConfig_Name = "saveConfig";
    const char *Webserver::SaveConfig_Text = "Konfiguration speichern";
    const char *Webserver::SaveConfig_Type = "bool";
    const char *Webserver::SaveConfig_Comment = "Save the current Config to ConfigFile";
    const char *Webserver::SaveConfig_BtnText = "SAVE";
    const char *Webserver::Time_Name = "time";
    const char *Webserver::Time_Text = "Systemzeit";
    const char *Webserver::Time_Comment = nullptr;

    /**
     * @brief Construct a new Webserver::Webserver object
     *
     * @param _HostnamePrefix Prefix for default Hostname ist not defined in Config
     * @param _Port Port of the Webserver if not defined in Config
     * @param _ConfUser Username for the System Sites
     * @param _ConfPassword Password for the System Sites
     * @param _Offset RTC Timeoffset in seconds
     */
    Webserver::Webserver (const char *_HostnamePrefix, uint16_t _Port, const char *_ConfUser, const char *_ConfPassword, unsigned long _Offset)
        : Protocol (ElementName), Server (_Port), Websocket ("/ws"), Rtc (_Offset) {
      sprintf (Hostname, "%s_%08X", _HostnamePrefix, ESP.getChipId ());
      Port = _Port;
      Reboot = false;
      strncpy (ConfUser, _ConfUser, sizeof (ConfUser));
      strncpy (ConfPassword, _ConfPassword, sizeof (ConfPassword));
      WsUpdateCycle = 1000;
      WsLastUpdate = millis ();
    }

    /**
     * @brief Construct a new Webserver::Webserver object
     * Username and Password for System Sites is set to Default
     * @param _HostnamePrefix Prefix for default Hostname ist not defined in Config
     * @param _Port Port of the Webserver if not defined in Config
     * @param _ConfUser Username for the System Sites
     * @param _ConfPassword Password for the System Sites
     */
    Webserver::Webserver (const char *_HostnamePrefix, uint16_t _Port, const char *_ConfUser, const char *_ConfPassword) : Webserver (_HostnamePrefix, _Port, _ConfUser, _ConfPassword, JCA_IOT_WEBSERVER_TIME_OFFSET) {
    }

    /**
     * @brief Construct a new Webserver::Webserver object
     * Username and Password for System Sites is set to Default
     * @param _HostnamePrefix Prefix for default Hostname ist not defined in Config
     * @param _Port Port of the Webserver if not defined in Config
     * @param _Offset RTC Timeoffset in seconds
     */
    Webserver::Webserver (const char *_HostnamePrefix, uint16_t _Port, unsigned long _Offset) : Webserver (_HostnamePrefix, _Port, JCA_IOT_WEBSERVER_DEFAULT_CONF_USER, JCA_IOT_WEBSERVER_DEFAULT_CONF_PASS, _Offset) {
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
     * @param _Offset RTC Timeoffset in seconds
     */
    Webserver::Webserver (unsigned long _Offset) : Webserver (JCA_IOT_WEBSERVER_DEFAULT_HOSTNAMEPREFIX, JCA_IOT_WEBSERVER_DEFAULT_PORT, _Offset) {
    }

    /**
     * @brief Construct a new Webserver::Webserver object
     * Username and Password for System Sites is set to Default
     * Hostname-Prefix and Webserver-Port ist set to Default
     */
    Webserver::Webserver () : Webserver (JCA_IOT_WEBSERVER_DEFAULT_HOSTNAMEPREFIX, JCA_IOT_WEBSERVER_DEFAULT_PORT) {
    }

    /**
     * @brief Read the Config-JSON and store the Data
     *
     * @return true Config-File exists and is valid
     * @return false Config-File didn't exists or is not a valid JSON File
     */
    bool Webserver::readConfig () {
      DynamicJsonDocument JsonDoc(1000);
      // Get Wifi-Config from File5
      File ConfigFile = LittleFS.open (JCA_IOT_WEBSERVER_CONFIGPATH, "r");
      if (ConfigFile) {
        Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Config File Found");
        DeserializationError Error = deserializeJson (JsonDoc, ConfigFile);
        if (!Error) {
          Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Deserialize Done");
          JsonObject Config = JsonDoc.as<JsonObject> ();
          //------------------------------------------------------
          // Read WiFi Config
          //------------------------------------------------------
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Config contains WiFi");
            JsonObject WiFiConfig = Config[JCA_IOT_WEBSERVER_CONFKEY_WIFI].as<JsonObject> ();
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_SSID)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found SSID");
              if (!Connector.setSsid (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_SSID].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] SSID invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_PASS)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found Password");
              if (!Connector.setPassword (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_PASS].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] Password invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_DHCP)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found DHCP");
              if (!Connector.setDHCP (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_DHCP].as<bool> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] DHCP invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_IP)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found IP");
              if (!Connector.setIP (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_IP].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] IP invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_GATEWAY)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found Gateway");
              if (!Connector.setGateway (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_GATEWAY].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] Gateway invalid");
              }
            }
            if (WiFiConfig.containsKey (JCA_IOT_WEBSERVER_CONFKEY_WIFI_SUBNET)) {
              Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "[WiFi] Found Subnet");
              if (!Connector.setSubnet (WiFiConfig[JCA_IOT_WEBSERVER_CONFKEY_WIFI_SUBNET].as<const char *> ())) {
                Debug.println (FLAG_ERROR, true, ObjectName, __func__, "[WiFi] Subnet invalid");
              }
            }
          }
          //------------------------------------------------------
          // Read Server Config
          //------------------------------------------------------
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_HOSTNAME)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Config contains Hostname");
            strncpy (Hostname, Config[JCA_IOT_WEBSERVER_CONFKEY_HOSTNAME].as<const char *> (), sizeof (Hostname));
          }
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_PORT)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Config contains Serverport");
            Port = Config[JCA_IOT_WEBSERVER_CONFKEY_PORT].as<uint16_t> ();
          }
          if (Config.containsKey (JCA_IOT_WEBSERVER_CONFKEY_SOCKETUPDATE)) {
            Debug.println (FLAG_CONFIG, true, ObjectName, __func__, "Config contains WebSocket Update");
            WsUpdateCycle = Config[JCA_IOT_WEBSERVER_CONFKEY_SOCKETUPDATE].as<uint32_t> ();
          }

        } else {
          Debug.print (FLAG_ERROR, true, ObjectName, __func__, "deserializeJson() failed: ");
          Debug.println (FLAG_ERROR, true, ObjectName, __func__, Error.c_str ());
          return false;
        }
        ConfigFile.close ();
      } else {
        Debug.println (FLAG_ERROR, true, ObjectName, __func__, "Config File NOT found");
        return false;
      }
      return true;
    }

    /**
     * @brief Set the Element Config
     * Only existing Tags will be updated
     * @param _Tags Array of Config-Tags ("config": [])
     */
    void Webserver::setConfig (JsonArray _Tags) {
      Debug.println (FLAG_CONFIG, false, ObjectName, __func__, "Set");
      for (JsonObject Tag : _Tags) {
        if (Tag[JsonTagName] == Hostname_Name) {
          strncpy (Hostname, Tag[JsonTagValue].as<const char *> (), sizeof (Hostname));
          if (Debug.print (FLAG_CONFIG, false, ObjectName, __func__, Hostname_Name)) {
            Debug.print (FLAG_CONFIG, false, ObjectName, __func__, DebugSeparator);
            Debug.println (FLAG_CONFIG, false, ObjectName, __func__, Hostname);
          }
        }
        if (Tag[JsonTagName] == WsUpdateCycle_Name) {
          WsUpdateCycle = Tag[JsonTagValue].as<uint32_t> ();
          if (Debug.print (FLAG_CONFIG, false, ObjectName, __func__, WsUpdateCycle_Name)) {
            Debug.print (FLAG_CONFIG, false, ObjectName, __func__, DebugSeparator);
            Debug.println (FLAG_CONFIG, false, ObjectName, __func__, WsUpdateCycle);
          }
        }
      }
    }

    /**
     * @brief Set the Element Data
     * currently not used
     * @param _Tags Array of Data-Tags ("data": [])
     */
    void Webserver::setData (JsonArray _Tags) {
    }

    /**
     * @brief Execute the Commands
     *
     * @param _Tags Array of Commands ("cmd": [])
     */
    void Webserver::setCmd (JsonArray _Tags) {
      Debug.println (FLAG_CONFIG, false, ObjectName, __func__, "Set");
      for (JsonObject Tag : _Tags) {
        if (Tag[JsonTagName] == TimeSync_Name) {
          setTime (Tag[JsonTagValue].as<uint32_t> ());
          if (Debug.print (FLAG_CONFIG, false, ObjectName, __func__, TimeSync_Name)) {
            Debug.print (FLAG_CONFIG, false, ObjectName, __func__, DebugSeparator);
            Debug.println (FLAG_CONFIG, false, ObjectName, __func__, getTime ());
          }
        }
        if (Tag[JsonTagName] == SaveConfig_Name) {
          if (Tag[JsonTagValue].as<bool> ()) {
            onSaveConfigCB();
          }
          if (Debug.print (FLAG_CONFIG, false, ObjectName, __func__, SaveConfig_Name)) {
            Debug.print (FLAG_CONFIG, false, ObjectName, __func__, DebugSeparator);
            Debug.println (FLAG_CONFIG, false, ObjectName, __func__, Tag[JsonTagValue].as<bool> ());
          }
        }
      }
    }

    /**
     * @brief Create a list of Config-Tags containing the current Value
     *
     * @param _Tags Array the Tags have to add
     */
    void Webserver::writeSetupConfig (File _SetupFile) {
      Debug.println (FLAG_CONFIG, false, ObjectName, __func__, "Get");
      _SetupFile.println (",\"" + String(JsonTagConfig) + "\":[");
      _SetupFile.println ("{" + createSetupTag (Hostname_Name, Hostname_Text, Hostname_Comment, false, Hostname) + "}");
      _SetupFile.println (",{" + createSetupTag (WsUpdateCycle_Name, WsUpdateCycle_Text, WsUpdateCycle_Comment, false, WsUpdateCycle_Unit, WsUpdateCycle) + "}");
      _SetupFile.println ("]");
    }

    /**
     * @brief Create a list of Data-Tags containing the current Value
     *
     * @param _Tags Array the Tags have to add
     */
    void Webserver::writeSetupData (File _SetupFile) {
      Debug.println (FLAG_CONFIG, false, ObjectName, __func__, "Get");
      _SetupFile.println (",\"" + String(JsonTagData) + "\":[");
      _SetupFile.println ("{" + createSetupTag (Time_Name, Time_Text, Time_Comment, true, getTime ()) + "}");
      _SetupFile.println ("]");
    }

    /**
     * @brief Create a list of Command-Informations
     *
     * @param _Tags Array the Command-Infos have to add
     */
    void Webserver::writeSetupCmdInfo (File _SetupFile) {
      Debug.println (FLAG_CONFIG, false, ObjectName, __func__, "Get");
      _SetupFile.println (",\"" + String(JsonTagCmdInfo) + "\":[");
      _SetupFile.println ("{" + createSetupCmdInfo (TimeSync_Name, TimeSync_Text, TimeSync_Comment, TimeSync_Type) + "}");
      _SetupFile.println (",{" + createSetupCmdInfo (SaveConfig_Name, SaveConfig_Text, SaveConfig_Comment, SaveConfig_Type, SaveConfig_BtnText) + "}");
      _SetupFile.println ("]");
    }

    void Webserver::createConfigValues (JsonObject &_Values) {
      _Values[Hostname_Name] = Hostname;
      _Values[WsUpdateCycle_Name] = WsUpdateCycle;
    }

    void Webserver::createDataValues (JsonObject &_Values) {
      _Values[Time_Name] = getTime ();
    }

    /**
     * @brief Define all Default Web-Requests and init the Webserver
     *
     * @return true Controller is connected to WiFI
     * @return false Controller runs as AP
     */
    bool Webserver::init () {
      // Read Config
      readConfig ();

      // WiFi Connection
      Connector.init ();

      // WebSocket - Init
      Websocket.onEvent ([this] (AsyncWebSocket *_Server, AsyncWebSocketClient *_Client, AwsEventType _Type, void *_Arg, uint8_t *_Data, size_t _Len) { this->onWsEvent (_Server, _Client, _Type, _Arg, _Data, _Len); });
      Server.addHandler (&Websocket);

      // Webserver - WiFi Config
      Server.on (JCA_IOT_WEBSERVER_PATH_CONNECT, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onWebConnectGet (_Request); });
      Server.on (JCA_IOT_WEBSERVER_PATH_CONNECT, HTTP_POST, [this] (AsyncWebServerRequest *_Request) { this->onWebConnectPost (_Request); });

      // Webserver - System Config
      Server.on (JCA_IOT_WEBSERVER_PATH_SYS, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onWebSystemGet (_Request); });
      Server.on (
          JCA_IOT_WEBSERVER_PATH_SYS_UPLOAD, HTTP_POST, [this] (AsyncWebServerRequest *_Request) { _Request->redirect (JCA_IOT_WEBSERVER_PATH_SYS); },
          [this] (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) { this->onWebSystemUploadData (_Request, _Filename, _Index, _Data, _Len, _Final); });
      Server.on (
          JCA_IOT_WEBSERVER_PATH_SYS_UPDATE, HTTP_POST, [this] (AsyncWebServerRequest *_Request) { this->onWebSystemUpdate (_Request); },
          [this] (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) { this->onWebSystemUpdateData (_Request, _Filename, _Index, _Data, _Len, _Final); });
      Server.on (JCA_IOT_WEBSERVER_PATH_SYS_RESET, HTTP_POST, [this] (AsyncWebServerRequest *_Request) { this->onWebSystemReset (_Request); });

      // Webserver - Custom Pages
      Server.on ("/", HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onWebHomeGet (_Request); });
      Server.on (JCA_IOT_WEBSERVER_PATH_HOME, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onWebHomeGet (_Request); });
      Server.on (JCA_IOT_WEBSERVER_PATH_CONFIG, HTTP_GET, [this] (AsyncWebServerRequest *_Request) { this->onWebConfigGet (_Request); });

      // RestAPI
      Server.on (
          "/api", HTTP_ANY,
          [this] (AsyncWebServerRequest *_Request) {
            Debug.println (FLAG_TRAFFIC, true, this->ObjectName, "RestAPI", "Request");
            DynamicJsonDocument JBuffer (1000);
            JsonVariant InData;

            DeserializationError Error = deserializeJson (JBuffer, (char *)(_Request->_tempObject));
            if (Error) {
              if (Debug.print (FLAG_ERROR, true, ObjectName, "RestAPI", "+ deserializeJson() failed: ")) {
                Debug.println (FLAG_ERROR, true, ObjectName, "RestAPI", Error.c_str ());
                Debug.print (FLAG_ERROR, true, ObjectName, "RestAPI", "+ Body:");
                Debug.println (FLAG_ERROR, true, ObjectName, "RestAPI", (char *)(_Request->_tempObject));
              }
              JBuffer.clear ();
            }
            InData = JBuffer.as<JsonVariant> ();
            this->onRestApiRequest (_Request, InData);
          },
          [this] (AsyncWebServerRequest *_Request, String _Filename, size_t _Index, uint8_t *_Data, size_t _Len, bool _Final) {
            Debug.println (FLAG_TRAFFIC, true, this->ObjectName, "RestAPI", "File");
          },
          [this] (AsyncWebServerRequest *_Request, uint8_t *_Data, size_t _Len, size_t _Index, size_t _Total) {
            Debug.println (FLAG_TRAFFIC, true, this->ObjectName, "RestAPI", "Data");

            if (_Total > 0 && _Request->_tempObject == nullptr) {
              _Request->_tempObject = malloc (_Total + 10);
            }
            if (_Request->_tempObject != nullptr) {
              memcpy ((uint8_t *)(_Request->_tempObject) + _Index, _Data, _Len);
            }
          });

      // Webserver - If not defined
      Server.serveStatic ("/", LittleFS, "/")
          .setDefaultFile (JCA_IOT_WEBSERVER_PATH_HOME);
      Server.onNotFound ([] (AsyncWebServerRequest *_Request) { _Request->redirect (JCA_IOT_WEBSERVER_PATH_SYS); });
      Server.begin ();

      Debug.println (FLAG_SETUP, true, ObjectName, __func__, "Done");
      return Connector.isConnected ();
    }

    /**
     * @brief Loop Function to handle Connection State
     *
     * @return true Controller is connected to WiFI
     * @return false Controller runs as AP
     */
    bool Webserver::handle () {
      uint32_t ActMillis = millis ();
      // Update Cycle WebSocket
      if (ActMillis - WsLastUpdate >= WsUpdateCycle && WsUpdateCycle > 0) {
        doWsUpdate (nullptr);
        WsLastUpdate = ActMillis;
      }
      // Check WiFi Connection
      Connector.handle ();
      return Connector.isConnected ();
    }

    void Webserver::update (struct tm &_Time) {
    }

    void Webserver::onSystemReset (SimpleCallback _CB) {
      onSystemResetCB = _CB;
    }

    void Webserver::onSaveConfig (SimpleCallback _CB) {
      onSaveConfigCB = _CB;
    }

    void Webserver::setTime (unsigned long _Epoch, int _Millis) {
      Rtc.setTime (_Epoch, _Millis);
    }
    void Webserver::setTime (int _Second, int _Minute, int _Hour, int _Day, int _Month, int _Year, int _Millis) {
      Rtc.setTime (_Second, _Minute, _Hour, _Day, _Month, _Year, _Millis);
    }
    void Webserver::setTimeStruct (tm _Time) {
      Rtc.setTimeStruct (_Time);
    }
    bool Webserver::timeIsValid () {
      return Rtc.getEpoch () > JCA_IOT_WEBSERVER_TIME_VALID;
    }
    tm Webserver::getTimeStruct () {
      return Rtc.getTimeStruct ();
    }
    String Webserver::getTime () {
      return Rtc.getTime ();
    }
    String Webserver::getDate () {
      return Rtc.getTime (JCA_IOT_WEBSERVER_TIME_DATEFORMAT);
    }
    String Webserver::getTimeString (String _Format) {
      if (_Format.length () == 0) {
        return Rtc.getTime (JCA_IOT_WEBSERVER_TIME_TIMEFORMAT);
      } else {
        return Rtc.getTime (_Format);
      }
    }
  }
}