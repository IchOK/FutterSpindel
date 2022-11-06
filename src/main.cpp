// Firmware
#include "FS.h"
#include <Arduino.h>
#include <LittleFS.h>
#define SPIFFS LittleFS

// Basics
#include <JCA_IOT_Webserver.h>
#include <JCA_SYS_DebugOut.h>

// Project function
#include <JCA_FNC_Feeder.h>
#include <JCA_FNC_Level.h>
#include <JCA_FNC_Parent.h>

using namespace JCA::IOT;
using namespace JCA::SYS;
using namespace JCA::FNC;
#define STAT_PIN LED_BUILTIN

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Custom Code
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define CONFIGPATH "/usrConfig.json"
//-------------------------------------------------------
// Feeder
//-------------------------------------------------------
#define EN_PIN D1   // Enable
#define STEP_PIN D2 // Step
#define DIR_PIN D3  // Direction

Feeder Spindel (EN_PIN, STEP_PIN, DIR_PIN, "Spindel");

//-------------------------------------------------------
// Level
//-------------------------------------------------------
#define LEVEL_PIN A0 // Levelsensor

Level Futter (LEVEL_PIN, "Futter");

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++
// JCA IOT Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++
Webserver Server;
//-------------------------------------------------------
// System Functions
//-------------------------------------------------------
void cbSystemReset () {
  ESP.restart ();
}
void cbSaveConfig () {
  File ConfigFile = LittleFS.open (CONFIGPATH, "w");
  bool ElementInit = false;
  ConfigFile.println("{\"elements\":[");
  Server.writeSetup(ConfigFile, ElementInit);
  Spindel.writeSetup (ConfigFile, ElementInit);
  Futter.writeSetup (ConfigFile, ElementInit);
  ConfigFile.println ("]}");
  ConfigFile.close ();
}

//-------------------------------------------------------
// Website Functions
//-------------------------------------------------------
String cbWebHomeReplace (const String &var) {
  return String ();
}
String cbWebConfigReplace (const String &var) {
  return String ();
}
//-------------------------------------------------------
// RestAPI Functions
//-------------------------------------------------------
void cbRestApiGet (JsonVariant &_In, JsonVariant &_Out) {
  JsonObject Elements = _Out.createNestedObject (Protocol::JsonTagElements);
  Server.getValues (Elements);
  Spindel.getValues (Elements);
  Futter.getValues (Elements);
}

void cbRestApiPost (JsonVariant &_In, JsonVariant &_Out) {
  if (_In.containsKey (Protocol::JsonTagElements)) {
    JsonArray Elements = (_In.as<JsonObject> ())[Protocol::JsonTagElements].as<JsonArray> ();
    Server.set (Elements);
    Spindel.set (Elements);
    Futter.set (Elements);
  }
}

void cbRestApiPut (JsonVariant &_In, JsonVariant &_Out) {
  _Out["message"] = "PUT not Used";
}

void cbRestApiPatch (JsonVariant &_In, JsonVariant &_Out) {
  cbSaveConfig();
}

void cbRestApiDelete (JsonVariant &_In, JsonVariant &_Out) {
  _Out["message"] = "DELETE not used";
}

//-------------------------------------------------------
// Websocket Functions
//-------------------------------------------------------
void cbWsUpdate (JsonVariant &_In, JsonVariant &_Out) {
  JsonObject Elements = _Out.createNestedObject (Protocol::JsonTagElements);
  Spindel.getValues (Elements);
  Futter.getValues (Elements);
}
void cbWsData (JsonVariant &_In, JsonVariant &_Out) {
  if (_In.containsKey (Protocol::JsonTagElements)) {
    JsonArray Elements = (_In.as<JsonObject> ())[Protocol::JsonTagElements].as<JsonArray> ();
    Spindel.set (Elements);
    Futter.set (Elements);
  }
  JsonObject Elements = _Out.createNestedObject (Protocol::JsonTagElements);
  Spindel.getValues (Elements);
  Futter.getValues (Elements);
}

//#######################################################
// Setup
//#######################################################
void setup () {
  DynamicJsonDocument JDoc(10000);

  pinMode (STAT_PIN, OUTPUT);
  digitalWrite (STAT_PIN, LOW);

  Debug.init (FLAG_NONE);
  //Debug.init (FLAG_ERROR | FLAG_SETUP | FLAG_CONFIG | FLAG_TRAFFIC);// | FLAG_LOOP);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Filesystem
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  if (!LittleFS.begin ()) {
    Debug.println (FLAG_ERROR, false, "root", "setup", "LITTLEFS Mount Failed");
    return;
  }

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // JCA IOT Functions - WiFiConnect
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // System
  Server.init ();
  Server.onSystemReset (cbSystemReset);
  Server.onSaveConfig (cbSaveConfig);
  // Web
  Server.onWebHomeReplace (cbWebHomeReplace);
  Server.onWebConfigReplace (cbWebConfigReplace);
  // RestAPI
  Server.onRestApiGet (cbRestApiGet);
  Server.onRestApiPost (cbRestApiPost);
  Server.onRestApiPut (cbRestApiPut);
  Server.onRestApiPatch (cbRestApiPatch);
  // Web-Socket
  Server.onWsData (cbWsData);
  Server.onWsUpdate (cbWsUpdate);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // Custom Code
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //-------------------------------------------------------
  // Read Config File
  //-------------------------------------------------------
  File ConfigFile = LittleFS.open (CONFIGPATH, "r");
  if (ConfigFile) {
    Debug.println (FLAG_CONFIG, false, "main", "setup", "Config File Found");
    DeserializationError Error = deserializeJson (JDoc, ConfigFile);
    if (!Error) {
      Debug.println (FLAG_CONFIG, false, "main", "setup", "Deserialize Done");
      if (JDoc.containsKey (Protocol::JsonTagElements)) {
        JsonArray Elements = (JDoc.as<JsonObject> ())[Protocol::JsonTagElements].as<JsonArray> ();
        Spindel.set (Elements);
        Futter.set (Elements);
      }
    } else {
      Debug.print (FLAG_ERROR, false, "main", "setup", "deserializeJson() failed: ");
      Debug.println (FLAG_ERROR, false, "main", "setup", Error.c_str ());
    }
    ConfigFile.close ();
  } else {
    Debug.println (FLAG_ERROR, false, "main", "setup", "Config File NOT found");
  }
}

//#######################################################
// Loop
//#######################################################
void loop () {
  Server.handle ();
  tm CurrentTime = Server.getTimeStruct ();

  Spindel.update (CurrentTime);
  Futter.update (CurrentTime);
}