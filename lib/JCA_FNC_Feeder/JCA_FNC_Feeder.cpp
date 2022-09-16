
#include <JCA_FNC_Feeder.h>
using namespace JCA::SYS;

namespace JCA {
  namespace FNC {
    Feeder::Feeder (uint8_t _PinEnable, uint8_t _PinStep, uint8_t _PinDir, const char *_Name)
        : Stepper (AccelStepper::DRIVER, _PinStep, _PinDir) {

      // Intern
      strncpy (ObjectName, _Name, sizeof (ObjectName));
      DoFeed = false;
      AutoFeedDone = false;

      // Konfig
      FeedingHour = -1;
      FeedingMinute = -1;
      SteppsPerRotation = 0.0;
      FeedingRotations = 0.0;
      Acceleration = 0.0;
      MaxSpeed = 0.0;
      ConstSpeed = 0.0;

      // Daten
      RunConst = false;
      Feeding = false;

      // Hardware
      Stepper.setEnablePin(_PinEnable);
      Stepper.setPinsInverted (false, false, true);
      Stepper.disableOutputs ();
    }

    void Feeder::update (struct tm &_Time) {
      const char *FunctionName = "update";
      bool AutoFeed = FeedingHour == _Time.tm_hour && FeedingMinute == _Time.tm_min && _Time.tm_year > 2000;

      // Run const Speed
      if (RunConst) {
        // Constant Mode
        Stepper.run ();
        DoFeed = false;
      } else {
        // Dosing Mode
        if ((AutoFeed && !AutoFeedDone) || DoFeed) {
          Debug.println (FLAG_LOOP, false, ObjectName, FunctionName, "Start Feeding");
          Stepper.move ((long)(SteppsPerRotation * FeedingRotations));
          Stepper.enableOutputs ();
          Feeding = true;
          DoFeed = false;
        }
        if (Stepper.distanceToGo () == 0 && Feeding) {
          Debug.println (FLAG_LOOP, false, ObjectName, FunctionName, "Done Feeding");
          Stepper.disableOutputs ();
          Feeding = false;
        }

        Stepper.run ();
      }
      AutoFeedDone = AutoFeed;
    }

    void Feeder::set (JsonObject _Collection) {
      if (_Collection.containsKey (ObjectName)) {
        JsonObject ObjectData = _Collection[ObjectName].as<JsonObject>();
        if (ObjectData.containsKey ("config")) {
          setConfig (ObjectData["config"].as<JsonObject> ());
        }
        if (ObjectData.containsKey ("data")) {
          setData (ObjectData["data"].as<JsonObject> ());
        }
      }
    }

    void Feeder::getConfig (JsonObject &_Collection) {
      const char *FunctionName = "getConfig";
      JsonObject ObjectData = _Collection.createNestedObject (ObjectName);
      JsonObject Config = ObjectData.createNestedObject ("config");
      createConfig(Config);
      if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "ObjectData:")) {
        String ObjectStr;
        serializeJson (ObjectData, ObjectStr);
        Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, ObjectStr);
      }
    }

    void Feeder::getData (JsonObject &_Collection) {
      const char *FunctionName = "getData";
      JsonObject ObjectData = _Collection.createNestedObject (ObjectName);
      JsonObject Data = ObjectData.createNestedObject ("data");
      createData (Data);
      if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "ObjectData:")) {
        String ObjectStr;
        serializeJson (ObjectData, ObjectStr);
        Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, ObjectStr);
      }
    }

    void Feeder::getAll (JsonObject &_Collection) {
      const char *FunctionName = "getAll";
      JsonObject ObjectData = _Collection.createNestedObject (ObjectName);
      JsonObject Config = ObjectData.createNestedObject ("config");
      JsonObject Data = ObjectData.createNestedObject ("data");
      createConfig (Config);
      createData (Data);
    }

    void Feeder::createConfig (JsonObject &_Data) {
      const char *FunctionName = "createConfig";
      Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, "Get");
      _Data["FeedingHour"] = FeedingHour;
      _Data["FeedingMinute"] = FeedingMinute;
      _Data["SteppsPerRotation"] = SteppsPerRotation;
      _Data["FeedingRotations"] = FeedingRotations;
      _Data["Acceleration"] = Acceleration;
      _Data["MaxSpeed"] = MaxSpeed;
      _Data["ConstSpeed"] = ConstSpeed;
    }

      void Feeder::createData (JsonObject & _Data) {
        const char *FunctionName = "createData";
        Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, "Get");
        _Data["Feeding"] = Feeding;
        _Data["DistanceToGo"] = Stepper.distanceToGo ();
        _Data["RunConst"] = RunConst;
        _Data["Speed"] = Stepper.speed ();
      }

      void Feeder::setConfig (JsonObject _Data) {
        const char *FunctionName = "setConfig";
        Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, "Set");
        if (_Data.containsKey ("FeedingHour")) {
          FeedingHour = _Data["FeedingHour"].as<int8_t> ();
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "FeedingHour:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, FeedingHour);
          }
        }
        if (_Data.containsKey ("FeedingMinute")) {
          FeedingHour = _Data["FeedingMinute"].as<int8_t> ();
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "FeedingMinute:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, FeedingHour);
          }
        }
        if (_Data.containsKey ("SteppsPerRotation")) {
          SteppsPerRotation = _Data["SteppsPerRotation"].as<int32_t> ();
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "SteppsPerRotation:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, SteppsPerRotation);
          }
        }
        if (_Data.containsKey ("FeedingRotations")) {
          FeedingRotations = _Data["FeedingRotations"].as<int32_t> ();
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "FeedingRotations:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, FeedingRotations);
          }
        }
        if (_Data.containsKey ("Acceleration")) {
          Acceleration = _Data["Acceleration"].as<float> ();
          Stepper.setAcceleration (Acceleration);
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "Acceleration:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, Acceleration);
          }
        }
        if (_Data.containsKey ("MaxSpeed")) {
          MaxSpeed = _Data["MaxSpeed"].as<float> ();
          Stepper.setMaxSpeed (MaxSpeed);
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "MaxSpeed:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, MaxSpeed);
          }
        }
        if (_Data.containsKey ("ConstSpeed")) {
          ConstSpeed = _Data["ConstSpeed"].as<float> ();
          Stepper.setSpeed (ConstSpeed);
          if (Debug.print (FLAG_CONFIG, false, ObjectName, FunctionName, "ConstSpeed:")) {
            Debug.println (FLAG_CONFIG, false, ObjectName, FunctionName, ConstSpeed);
          }
        }
      }

      void Feeder::setData (JsonObject _Data) {
        const char *FunctionName = "setData";
        if (_Data.containsKey ("runConst")) {
          RunConst = _Data["runConst"].as<bool> ();
          if (Debug.print (FLAG_LOOP, false, ObjectName, FunctionName, "RunConst:")) {
            Debug.println (FLAG_LOOP, false, ObjectName, FunctionName, RunConst);
          }
          if (RunConst) {
            Stepper.enableOutputs ();
          } else {
            Stepper.disableOutputs ();
          }
        }

        if (_Data.containsKey ("doFeed")) {
          DoFeed = _Data["doFeed"].as<bool> ();
          if (Debug.print (FLAG_LOOP, false, ObjectName, FunctionName, "DoFeed:")) {
            Debug.println (FLAG_LOOP, false, ObjectName, FunctionName, DoFeed);
          }
          if (DoFeed) {
            RunConst = false;
          }
        }
      }
    }
}
