//############################# Plugin 165: Serial MCU controlled switch ###############################
//
//  based on P020 Ser2Net and P001 Switch
//
//  designed for TUYA/YEWELINK Wifi Touch Light switch with ESP8266 + PIC16F1829 MCU and
//  also the similar Sonoff Dual MCU controlled Wifi relay
//
//  Dummy device (Switch) must be used to control relays, one device per relay.. with Domoticz at least.
//  Every relay needs unique idx for control, and rules, i have no other idea.
//
//#######################################################################################################

#define PLUGIN_165
#define PLUGIN_ID_165         165
#define PLUGIN_NAME_165       "Serial MCU controlled switch"
#define PLUGIN_VALUENAME1_165 "Relay0"
#define PLUGIN_VALUENAME2_165 "Relay1"
#define PLUGIN_VALUENAME3_165 "Relay2"

#define BUFFER_SIZE   128

#define SER_SWITCH_YEWE 1
#define SER_SWITCH_SONOFFDUAL 2

static byte switchstate[3];
static byte ostate[3];
byte commandstate = 0; // 0:no,1:inprogress,2:finished
byte numrelay = 1;
boolean Plugin_165_init = false;

boolean Plugin_165(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_165;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
        Device[deviceCount].VType = SENSOR_TYPE_SWITCH;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].ValueCount = 3;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }
    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_165);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_165));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_165));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_165));
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        byte choice = Settings.TaskDevicePluginConfig[event->TaskIndex][0];
        String options[2];
        options[0] = F("Yewelink/TUYA");
        options[1] = F("Sonoff Dual");
        int optionValues[2] = { SER_SWITCH_YEWE, SER_SWITCH_SONOFFDUAL };
        addFormSelector(string, F("Switch Type"), F("plugin_165_type"), 2, options, optionValues, choice);

        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_YEWE)
        {
          choice = Settings.TaskDevicePluginConfig[event->TaskIndex][1];
          String buttonOptions[3];
          buttonOptions[0] = F("1");
          buttonOptions[1] = F("2");
          buttonOptions[2] = F("3");
          addFormSelector(string, F("Number of relays"), F("plugin_165_button"), 3, buttonOptions, NULL, choice);
        }

        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("plugin_165_type"));
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_YEWE)
        {
          Settings.TaskDevicePluginConfig[event->TaskIndex][1] = getFormItemInt(F("plugin_165_button"));
        }

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        Serial.setDebugOutput(false);
        Serial.setRxBufferSize(BUFFER_SIZE); // Arduino core for ESP8266 WiFi chip 2.4.0
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_YEWE)
        {
          numrelay = Settings.TaskDevicePluginConfig[event->TaskIndex][1];
          Serial.begin(9600, SERIAL_8N1);
          delay(10);
          getmcustate(); // request status on startup
        }
        if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_SONOFFDUAL)
        {
          numrelay = 2;
          Serial.begin(19230, SERIAL_8N1);
          // on start we do not know the state of the relays...
        }

        success = true;
        Plugin_165_init = true;
        break;
      }


    case PLUGIN_SERIAL_IN:
      {

        int bytes_read = 0;
        byte serial_buf[BUFFER_SIZE];
        String log;

        if (Plugin_165_init)
        {
          while (Serial.available() > 0) {
            yield();
            if (bytes_read < BUFFER_SIZE) {
              serial_buf[bytes_read] = Serial.read();

              if (bytes_read == 0) { // packet start

                commandstate = 0;
                switch (Settings.TaskDevicePluginConfig[event->TaskIndex][0])
                {
                  case SER_SWITCH_YEWE:
                    {
                      if (serial_buf[bytes_read] == 0x55) {
                        commandstate = 1;
                      }
                      break;
                    }
                  case SER_SWITCH_SONOFFDUAL:
                    {
                      if (serial_buf[bytes_read] == 0xA0) {
                        commandstate = 1;
                      }
                      break;
                    }
                }
              } else {

                if (commandstate == 1) {

                  if (bytes_read == 1) { // packet valid?
                    switch (Settings.TaskDevicePluginConfig[event->TaskIndex][0])
                    {
                      case SER_SWITCH_YEWE:
                        {
                          if (serial_buf[bytes_read] != 0xAA) {
                            commandstate = 0;
                            bytes_read = 0;
                          }
                          break;
                        }
                      case SER_SWITCH_SONOFFDUAL:
                        {
                          if (serial_buf[bytes_read] != 0x04) {
                            commandstate = 0;
                            bytes_read = 0;
                          }
                          break;
                        }
                    }
                  }

                  if ( (bytes_read == 2) && (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_SONOFFDUAL)) {
                    ostate[0] = switchstate[0]; ostate[1] = switchstate[1];
                    switchstate[0] = 0; switchstate[1] = 0;
                    if ((serial_buf[bytes_read] == 1) || (serial_buf[bytes_read] == 3)) {
                      switchstate[0] = 1;
                    }
                    if ((serial_buf[bytes_read] == 2) || (serial_buf[bytes_read] == 3)) {
                      switchstate[1] = 1;
                    }
                    commandstate = 2; bytes_read = 0;
                    log = F("SW   : State ");
                    if (ostate[0] != switchstate[0]) {
                      UserVar[event->BaseVarIndex] = switchstate[0];
                      log += F(" r0:");
                      log += switchstate[0];
                    }
                    if (ostate[1] != switchstate[1]) {
                      UserVar[event->BaseVarIndex + 1] = switchstate[1];
                      log += F(" r1:");
                      log += switchstate[1];
                    }
                    addLog(LOG_LEVEL_INFO, log);
                    if ( (ostate[0] != switchstate[0]) || (ostate[1] != switchstate[1]) ) {
                      event->sensorType = SENSOR_TYPE_SWITCH;
                      sendData(event);
                    }
                  }
                  if (Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_YEWE) {
                    if ((bytes_read == 3) && (serial_buf[bytes_read] != 7))
                    {
                      commandstate = 0;  // command code 7 means status reporting, we do not need other packets
                      bytes_read = 0;
                    }
                    if (bytes_read == 10) {
                      byte btnnum = (serial_buf[6] - 1);
                      ostate[btnnum] = switchstate[btnnum];
                      switchstate[btnnum] = serial_buf[10];
                      commandstate = 2; bytes_read = 0;

                      if (ostate[btnnum] != switchstate[btnnum]) {
                        log = F("SW   : State");
                        switch (btnnum) {
                          case 0: {
                              if (numrelay > 1) {
                                UserVar[event->BaseVarIndex + btnnum] = switchstate[btnnum];
                                log += F(" r0:");
                                log += switchstate[btnnum];
                              }

                            }
                          case 1: {
                              if (numrelay != 2) {
                                UserVar[event->BaseVarIndex + btnnum] = switchstate[btnnum];
                                log += F(" r1:");
                                log += switchstate[btnnum];
                              }

                            }
                          case 2: {
                              if (numrelay > 1) {
                                UserVar[event->BaseVarIndex + btnnum] = switchstate[btnnum];
                                log += F(" r2:");
                                log += switchstate[btnnum];
                              }

                            }
                        }
                        event->sensorType = SENSOR_TYPE_SWITCH;
                        addLog(LOG_LEVEL_INFO, log);
                        sendData(event);
                      }
                    } //10th byte


                  } // yewe decode
                } // commandstate 1 end
              } // end of status decoding

              if (commandstate == 1) {
                bytes_read++;
              }
            } else
              Serial.read(); // if buffer full, dump incoming
          }
        } // plugin initialized
        success = true;
        break;
      }

    case PLUGIN_READ:
      {
        if (Plugin_165_init)
        {
          if ((Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_YEWE) && (commandstate != 1))
          {
            String log = F("SW   : ReadState");
            addLog(LOG_LEVEL_INFO, log);
            getmcustate();
          }
          success = true;
        }
        break;
      }

    case PLUGIN_WRITE:
      {
        String log = "";
        String command = parseString(string, 1);
        byte rnum = 0;
        byte rcmd = 0;

        if (Plugin_165_init)
        {

          if ( command == F("relay") )
          {
            success = true;

            if ((event->Par1 >= 0) && (event->Par1 < numrelay)) {
              rnum = event->Par1;
            }
            if ((event->Par2 == 0) || (event->Par2 == 1)) {
              rcmd = event->Par2;
            }

            sendmcucommand(rnum, rcmd, Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
            if ( Settings.TaskDevicePluginConfig[event->TaskIndex][0] == SER_SWITCH_SONOFFDUAL) {
              UserVar[(event->BaseVarIndex + rnum)] = switchstate[rnum];
              event->sensorType = SENSOR_TYPE_SWITCH;
              sendData(event);
            }
            String log = F("SW   : SetSwitch r");
            log += event->Par1;
            log += F(":");
            log += rcmd;
            addLog(LOG_LEVEL_INFO, log);

          }
        }
        break;
      }

  }
  return success;
}

void getmcustate() {
  Serial.write(0x55); // request status information
  Serial.write(0xAA);
  Serial.write(0x00);
  Serial.write(0x08);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x07);
  Serial.flush();
}

void sendmcucommand(byte btnnum, byte state, byte swtype) // btnnum=0,1,2, state=0/1
{
  byte sstate;

  switch (swtype)
  {
    case SER_SWITCH_YEWE:
      {
        Serial.write(0x55); // header 55AA
        Serial.write(0xAA);
        Serial.write(0x00); // version 00
        Serial.write(0x06); // command 06 - send order
        Serial.write(0x00);
        Serial.write(0x05); // following data length 0x05
        Serial.write( (btnnum + 1) ); // relay number 1,2,3
        Serial.write(0x01); // ?
        Serial.write(0x00); // ?
        Serial.write(0x01); // ?
        Serial.write( state ); // status
        Serial.write((13 + btnnum + state)); // checksum:sum of all bytes in packet mod 256
        Serial.flush();
        break;
      }
    case SER_SWITCH_SONOFFDUAL:
      {
        if ((btnnum == 0) || (btnnum == 1)) {
          switchstate[btnnum] = state;
          Serial.write(0xA0);
          Serial.write(0x04);
          if (btnnum == 0) {
            sstate = state + (switchstate[1] * 2);
          } else {
            sstate = (state * 2) + switchstate[0];
          }
          Serial.write( sstate );
          Serial.write(0xA1);
          Serial.flush();
          break;
        }
      }
  }
}
