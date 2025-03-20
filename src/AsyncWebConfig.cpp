/*
Forked by SandroFTW
Version 1.0

File AsyncWebConfig.cpp
Version 1.0.2
Author Gerald Lechner
contakt lechge@gmail.com

Description
This library builds a web page with a smart phone friendly form to edit
a free definable number of configuration parameters.
The submitted data will bestored in the SPIFFS
The library works with ESP8266 and ESP32

Dependencies:
  ESPAsyncWebServer.h
  ArduinoJson.h

*/

/*
COLOR PALETTE:
Almost Black: #222831 (Input fields backfround)
Dark Grey: #393E46 (Background)
Turquoise: #00ADB5 (<hr> seperator lines, small features)
Grey/white: #EEEEEE (Text)
*/
#include <AsyncWebConfig.h>
#include <Arduino.h>
#include <FS.h>

#if defined(ESP32)
  #include "SPIFFS.h"
#endif

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

const char * inputtypes[] = {"text","password","number","date","time","range","check","radio","select","color","float"};

//HTML templates
//Template for header and begin of form
const char HTML_START[] PROGMEM =
"<!DOCTYPE HTML>\n"
"<html lang='de'>\n"
"<head>\n"
"<meta http-equiv='Content-Type' content='text/html; charset=utf-8'>\n"
"<meta name='viewport' content='width=320' />\n"
"<title>QShift32 Config</title>\n"
"<style>\n"
"body {\n"
"  background-color: #393E46;\n"
"  font-family: Arial, Helvetica, Sans-Serif;\n"
"  Color: #EEEEEE;\n"
"  font-size:12pt;\n"
"  width:320px;\n"
"}\n"
".titel {\n"
"  font-weight:bold;\n"
"  text-align:center;\n"
"  width:100%%;\n"
"  padding:5px;\n"
"}\n"
".zeile {\n"
"  width:100%%;\n"
"  padding:5px;\n"
"  text-align: center;\n"
"  accent-color: #00ADB5\n"
"}\n"
"button {\n"
"  font-size:16pt;\n"
"  color: #222831\n"
"  width:150px;\n"
"  border-radius:4px;\n"
"  margin:10px;\n"
"}\n"
"textarea {\n"
"  border: 2px solid #00ADB5\n"
"  border-radius: 4px\n"
"  color: #EEEEEE\n"
"  background-color: #222831\n"
"}\n"
"hr {\n"
"  border:2px solid #00ADB5\n"
"}\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div id='main_div' style='margin-left:15px;margin-right:15px;'>\n"
"<div class='titel'>%s</div>\n"
"<hr>\n"
"<form method='post'>\n";

//Template for one input field
const char HTML_ENTRY_SIMPLE[] PROGMEM =
"  <div class='zeile'><b>%s</b></div>\n"
"  <div class='zeile'><input type='%s' value='%s' name='%s'></div>\n";
const char HTML_ENTRY_AREA[] PROGMEM =
"  <div class='zeile'><b>%s</b></div>\n"
"  <div class='zeile'><textarea rows='%i' cols='%i' name='%s'>%s</textarea></div>\n";
const char HTML_ENTRY_NUMBER[] PROGMEM =
"  <div class='zeile'><b>%s</b></div>\n"
"  <div class='zeile'><input type='number' min='%i' max='%i' value='%s' name='%s'></div>\n";
const char HTML_ENTRY_RANGE[] PROGMEM =
"  <div class='zeile'><b>%s</b></div>\n"
"  <div class='zeile'>%i&nbsp;<input type='range' min='%i' max='%i' value='%s' name='%s'>&nbsp;%i</div>\n";
const char HTML_ENTRY_CHECKBOX[] PROGMEM =
"  <div class='zeile'><b>%s </b><input type='checkbox' %s name='%s'></div>\n";
const char HTML_ENTRY_RADIO_TITLE[] PROGMEM =
" <div class='zeile'><b>%s</b></div>\n";
const char HTML_ENTRY_RADIO[] =
"  <div class='zeile'><input type='radio' name='%s' value='%s' %s>%s</div>\n";
const char HTML_ENTRY_SELECT_START[] PROGMEM =
" <div class='zeile'><b>%s</b></div>\n"
" <div class='zeile'><select name='%s'>\n";
const char HTML_ENTRY_SELECT_OPTION[] PROGMEM =
"  <option value='%s' %s>%s</option>\n";
const char HTML_ENTRY_SELECT_END[] PROGMEM =
" </select></div>\n";
const char HTML_ENTRY_MULTI_START[] PROGMEM =
" <div class='zeile'><b>%s</b></div>\n"
" <div class='zeile'><fieldset style='text-align:left;'>\n";
const char HTML_ENTRY_MULTI_OPTION[] PROGMEM =
"  <input type='checkbox' name='%s', value='%i' %s>%s<br>\n";
const char HTML_ENTRY_MULTI_END[] PROGMEM =
" </fieldset></div>\n";

//Template for save button and end of the form with save
const char HTML_END[] PROGMEM =
"<div class='zeile'><button type='submit' name='SAVE'>Save</button>\n"
"<button type='submit' name='RST'>Restart</button></div>\n"
"</form>\n"
"</div>\n"
"</body>\n"
"</html>\n";
//Template for save button and end of the form without save
const char HTML_BUTTON[] PROGMEM =
"<button type='submit' name='%s'>%s</button>\n";

AsyncWebConfig::AsyncWebConfig() {
  _apName = "";
};

void AsyncWebConfig::setDescription(String parameter){
  _count = 0;
  addDescription(parameter);
}

void AsyncWebConfig::addDescription(String parameter){
  DeserializationError error;
  const int capacity = JSON_ARRAY_SIZE(MAXVALUES)
  + MAXVALUES*JSON_OBJECT_SIZE(8);
  DynamicJsonDocument doc(capacity);
  char tmp[40];
  error = deserializeJson(doc,parameter);
  if (error ) {
    Serial.println(parameter);
    Serial.print("JSON AddDescription: ");
    Serial.println(error.c_str());
  } else {
    JsonArray array = doc.as<JsonArray>();
    uint8_t j = 0;
    for (JsonObject obj : array) {
      if (_count<MAXVALUES) {
        _description[_count].optionCnt = 0;
        if (obj.containsKey("name")) strlcpy(_description[_count].name,obj["name"],NAMELENGTH);
        if (obj.containsKey("label"))strlcpy(_description[_count].label,obj["label"],LABELLENGTH);
        if (obj.containsKey("type")) {
          if (obj["type"].is<const char *>()) {
            uint8_t t = 0;
            strlcpy(tmp,obj["type"],30);
            while ((t<INPUTTYPES) && (strcmp(tmp,inputtypes[t])!=0)) t++;
            if (t>INPUTTYPES) t = 0;
            _description[_count].type = t;
          } else {
            _description[_count].type = obj["type"];
          }
        } else {
          _description[_count].type = INPUTTEXT;
        }
        _description[_count].max = (obj.containsKey("max"))?obj["max"] :100;
        _description[_count].min = (obj.containsKey("min"))?obj["min"] : 0;
        if (obj.containsKey("default")) {
          strlcpy(tmp,obj["default"],30);
          values[_count] = String(tmp);
        } else {
          values[_count]="0";
        }
        if (obj.containsKey("options")) {
          JsonArray opt = obj["options"].as<JsonArray>();
          j = 0;
          for (JsonObject o : opt) {
            if (j<MAXOPTIONS) {
              _description[_count].options[j] = o["v"].as<String>();
              _description[_count].labels[j] = o["l"].as<String>();
            }
            j++;
          }
          _description[_count].optionCnt = opt.size();
        }
      }
      _count++;
    }

  }
  _apName = WiFi.macAddress();
  _apName.replace(":","");
  if (!SPIFFS.begin()) {
    SPIFFS.format();
    SPIFFS.begin();
  }
};

void createSimple(char * buf, const char * name, const char * label, const char * type, String value) {
  sprintf(buf,HTML_ENTRY_SIMPLE,label,type,value.c_str(),name);
}

void createTextarea(char * buf, DESCRIPTION descr, String value) {
  //max = rows min = cols
  sprintf(buf,HTML_ENTRY_AREA,descr.label,descr.max,descr.min,descr.name, value.c_str());
}

void createNumber(char * buf, DESCRIPTION descr, String value) {
  sprintf(buf,HTML_ENTRY_NUMBER,descr.label,descr.min,descr.max, value.c_str(),descr.name);
}

void createRange(char * buf, DESCRIPTION descr, String value) {
  sprintf(buf,HTML_ENTRY_RANGE,descr.label,descr.min,descr.min,descr.max,value.c_str(),descr.name,descr.max);
}

void createCheckbox(char * buf , DESCRIPTION descr, String value) {
  if (value != "0") {
    sprintf(buf,HTML_ENTRY_CHECKBOX,descr.label,"checked",descr.name);
  } else {
    sprintf(buf,HTML_ENTRY_CHECKBOX,descr.label,"",descr.name);
  }
}

void createRadio(char * buf , DESCRIPTION descr, String value, uint8_t index) {
  if (value == descr.options[index]) {
    sprintf(buf,HTML_ENTRY_RADIO,descr.name,descr.options[index].c_str(),"checked",descr.labels[index].c_str());
  } else {
    sprintf(buf,HTML_ENTRY_RADIO,descr.name,descr.options[index].c_str(),"",descr.labels[index].c_str());
  }
}

void startSelect(char * buf , DESCRIPTION descr) {
  sprintf(buf,HTML_ENTRY_SELECT_START,descr.label,descr.name);
}

void addSelectOption(char * buf, String option, String label, String value) {
  if (option == value) {
    sprintf(buf,HTML_ENTRY_SELECT_OPTION,option.c_str(),"selected",label.c_str());
  } else {
    sprintf(buf,HTML_ENTRY_SELECT_OPTION,option.c_str(),"",label.c_str());
  }
}

void startMulti(char * buf , DESCRIPTION descr) {
  sprintf(buf,HTML_ENTRY_MULTI_START,descr.label);
}

void addMultiOption(char * buf, String name, uint8_t option, String label, String value) {
  if ((value.length() > option) && (value[option] == '1')) {
    sprintf(buf,HTML_ENTRY_MULTI_OPTION,name.c_str(),option,"checked",label.c_str());
  } else {
    sprintf(buf,HTML_ENTRY_MULTI_OPTION,name.c_str(),option,"",label.c_str());
  }
}

//***********Different type for ESP32 WebServer and ESP8266WebServer ********
//both classes have the same functions
  //function to respond a HTTP request for the form use the default file
  //to save and restart ESP after saving the new config
  void AsyncWebConfig::handleFormRequest(AsyncWebServerRequest *request){
    handleFormRequest(request,CONFFILE);
  }
  //function to respond a HTTP request for the form use the filename
  //to save. If auto is true restart ESP after saving the new config
  void AsyncWebConfig::handleFormRequest(AsyncWebServerRequest *request, const char * filename){
//******************** Rest of the function has no difference ***************
  uint8_t a,v;
  String val;
  if (request->args() > 0) {
    if (request->hasArg(F("apName"))) _apName = request->arg(F("apName"));
    for (uint8_t i= 0; i < _count; i++) {
      if (_description[i].type == INPUTCHECKBOX) {
        values[i] = "0";
        if (request->hasArg(_description[i].name)) values[i] = "1";
      }  else if (_description[i].type == INPUTMULTICHECK) {
        values[i]="";
        for (a=0; a<_description[i].optionCnt; a++) values[i] += "0"; //clear result
        for (a=0; a< request->args(); a++) {
          if (request->argName(a) == _description[i].name) {
            val = request->arg(a);
            v= val.toInt();
            values[i].setCharAt(v,'1');
          }
        }
      } else {
        if (request->hasArg(_description[i].name)) values[i] = request->arg(_description[i].name) ;
      }
    }
    if (request->hasArg(F("SAVE")) || request->hasArg(F("RST"))) {
      writeConfig(filename);
      if (request->hasArg(F("RST"))) ESP.restart();
    }
  }
  boolean exit = false;
  if (request->hasArg(F("SAVE")) && _onSave) {
    _onSave(getResults());
    exit = true;
  }
  if (request->hasArg(F("DONE")) && _onDone) {
    _onDone(getResults());
    exit = true;
  }
  if (request->hasArg(F("CANCEL")) && _onCancel) {
    _onCancel();
    exit = true;
  }
  if (request->hasArg(F("DELETE")) && _onDelete) {
    _onDelete(_apName);
    exit = true;
  }
  if (!exit) {
    //request->setContentLength(CONTENT_LENGTH_UNKNOWN);
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    sprintf(_buf,HTML_START,_apName.c_str());
    response->print(_buf);
    if (_buttons == BTN_CONFIG) {
      createSimple(_buf,"apName","Name des Accesspoints","text",_apName);
      response->print(_buf);
    }

    for (uint8_t i = 0; i<_count; i++) {
      switch (_description[i].type) {
        case INPUTFLOAT:
        case INPUTTEXT: createSimple(_buf,_description[i].name,_description[i].label,"text",values[i]);
          break;
        case INPUTTEXTAREA: createTextarea(_buf,_description[i],values[i]);
            break;
        case INPUTPASSWORD: createSimple(_buf,_description[i].name,_description[i].label,"password",values[i]);
          break;
        case INPUTDATE: createSimple(_buf,_description[i].name,_description[i].label,"date",values[i]);
          break;
        case INPUTTIME: createSimple(_buf,_description[i].name,_description[i].label,"time",values[i]);
          break;
        case INPUTCOLOR: createSimple(_buf,_description[i].name,_description[i].label,"color",values[i]);
          break;
        case INPUTNUMBER: createNumber(_buf,_description[i],values[i]);
          break;
        case INPUTRANGE: createRange(_buf,_description[i],values[i]);
          break;
        case INPUTCHECKBOX: createCheckbox(_buf,_description[i],values[i]);
          break;
        case INPUTRADIO: sprintf(_buf,HTML_ENTRY_RADIO_TITLE,_description[i].label);
          for (uint8_t j = 0 ; j<_description[i].optionCnt; j++) {
            response->print(_buf);
            createRadio(_buf,_description[i],values[i],j);
          }
          break;
        case INPUTSELECT: startSelect(_buf,_description[i]);
          for (uint8_t j = 0 ; j<_description[i].optionCnt; j++) {
            response->print(_buf);
            addSelectOption(_buf,_description[i].options[j],_description[i].labels[j],values[i]);
          }
          response->print(_buf);
          strcpy_P(_buf,HTML_ENTRY_SELECT_END);
          break;
        case INPUTMULTICHECK: startMulti(_buf,_description[i]);
          for (uint8_t j = 0 ; j<_description[i].optionCnt; j++) {
            response->print(_buf);
            addMultiOption(_buf,_description[i].name,j,_description[i].labels[j],values[i]);
          }
          response->print(_buf);
          strcpy_P(_buf,HTML_ENTRY_MULTI_END);
          break;
        default : _buf[0] = 0;
          break;

      }
      response->print(_buf);
    }
    if (_buttons == BTN_CONFIG) {
      sprintf(_buf,HTML_END);
      response->print(_buf);
    } else {
      response->print("<div class='zeile'>\n");
      if ((_buttons & BTN_DONE) == BTN_DONE) {
        sprintf(_buf,HTML_BUTTON,"DONE","Done");
        response->print(_buf);
      }
      if ((_buttons & BTN_CANCEL) == BTN_CANCEL) {
        sprintf(_buf,HTML_BUTTON,"CANCEL","Cancel");
        response->print(_buf);
      }
      if ((_buttons & BTN_DELETE) == BTN_DELETE) {
        sprintf(_buf,HTML_BUTTON,"DELETE","Delete");
        response->print(_buf);
      }
      response->print("</div></form></div></body></html>\n");
    }
    request->send(response);
  }
}
//get the index for a value by parameter name
int16_t AsyncWebConfig::getIndex(const char * name){
  int16_t i = _count-1;
  while ((i>=0) && (strcmp(name,_description[i].name)!=0)) {
    i--;
  }
  return i;
}
//read configuration from file
boolean AsyncWebConfig::readConfig(const char * filename){
  String line,name,value;
  uint8_t pos;
  int16_t index;
  if (!SPIFFS.exists(filename)) {
    //if configfile does not exist write default values
    writeConfig(filename);
  }
  File f = SPIFFS.open(filename,"r");
  if (f) {
    Serial.println(F("Read configuration"));
    uint32_t size = f.size();
    while (f.position() < size) {
      line = f.readStringUntil(10);
      pos = line.indexOf('=');
      name = line.substring(0,pos);
      value = line.substring(pos+1);
      if ((name == "apName") && (value != "")) {
        _apName = value;
        Serial.println(line);
      } else {
        index = getIndex(name.c_str());
        if (!(index < 0)) {
          value.replace("~","\n");
          values[index] = value;
          if (_description[index].type == INPUTPASSWORD) {
            Serial.printf("%s=*************\n",_description[index].name);
          } else {
            Serial.println(line);
          }
        }
      }
    }
    f.close();
    return true;
  } else {
    Serial.println(F("Cannot read configuration"));
    return false;
  }
}
//read configuration from default file
boolean AsyncWebConfig::readConfig(){
  return readConfig(CONFFILE);
}
//write configuration to file
boolean AsyncWebConfig::writeConfig(const char * filename){
  String val;
  Serial.println("Save configuration");
  File f = SPIFFS.open(filename,"w");
  if (f) {
    f.printf("apName=%s\n",_apName.c_str());
    for (uint8_t i = 0; i<_count; i++){
      val = values[i];
      val.replace("\n","~");
      f.printf("%s=%s\n",_description[i].name,val.c_str());
    }
    return true;
  } else {
    Serial.println(F("Cannot write configuration"));
    return false;
  }

}
//write configuration to default file
boolean AsyncWebConfig::writeConfig(){
  return writeConfig(CONFFILE);
}
//delete configuration file
boolean AsyncWebConfig::deleteConfig(const char * filename){
  return SPIFFS.remove(filename);
}
//delete default configutation file
boolean AsyncWebConfig::deleteConfig(){
  return deleteConfig(CONFFILE);
}

//get a parameter value by its name
const String AsyncWebConfig::getString(const char * name) {
  int16_t index;
  index = getIndex(name);
  if (index < 0) {
    return "";
  } else {
    return values[index];
  }
}


//Get results as a JSON string
String AsyncWebConfig::getResults(){
  char buffer[1024];
  StaticJsonDocument<1000> doc;
  for (uint8_t i = 0; i<_count; i++) {
    switch (_description[i].type) {
      case INPUTPASSWORD :
      case INPUTSELECT :
      case INPUTDATE :
      case INPUTTIME :
      case INPUTRADIO :
      case INPUTCOLOR :
      case INPUTTEXT : doc[_description[i].name] = values[i]; break;
      case INPUTCHECKBOX :
      case INPUTRANGE :
      case INPUTNUMBER : doc[_description[i].name] = values[i].toInt(); break;
      case INPUTFLOAT : doc[_description[i].name] = values[i].toFloat(); break;

    }
  }
  serializeJson(doc,buffer);
  return String(buffer);
}

//Ser values from a JSON string
void AsyncWebConfig::setValues(String json){
  int val;
  float fval;
  char sval[255];
  DeserializationError error;
  StaticJsonDocument<1000> doc;
  error = deserializeJson(doc, json);
  if (error ) {
    Serial.print("JSON: ");
    Serial.println(error.c_str());
  } else {
    for (uint8_t i = 0; i<_count; i++) {
      if (doc.containsKey(_description[i].name)){
        switch (_description[i].type) {
          case INPUTPASSWORD :
          case INPUTSELECT :
          case INPUTDATE :
          case INPUTTIME :
          case INPUTRADIO :
          case INPUTCOLOR :
          case INPUTTEXT : strlcpy(sval,doc[_description[i].name],255);
            values[i] = String(sval); break;
          case INPUTCHECKBOX :
          case INPUTRANGE :
          case INPUTNUMBER : val = doc[_description[i].name];
            values[i] = String(val); break;
          case INPUTFLOAT : fval = doc[_description[i].name];
            values[i] = String(fval); break;

        }
      }
    }
  }
}


const char * AsyncWebConfig::getValue(const char * name){
  int16_t index;
  index = getIndex(name);
  if (index < 0) {
    return "";
  } else {
    return values[index].c_str();
  }
}

int AsyncWebConfig::getInt(const char * name){
  return getString(name).toInt();
}

float AsyncWebConfig::getFloat(const char * name){
  return getString(name).toFloat();
}

boolean AsyncWebConfig::getBool(const char * name){
  return (getString(name) != "0");
}

//get the accesspoint name
const char * AsyncWebConfig::getApName(){
  return _apName.c_str();
}
//get the number of parameters
uint8_t AsyncWebConfig::getCount(){
  return _count;
}

//get the name of a parameter
String AsyncWebConfig::getName(uint8_t index){
  if (index < _count) {
    return String(_description[index].name);
  } else {
    return "";
  }
}

//set the value for a parameter
void AsyncWebConfig::setValue(const char*name,String value){
  int16_t i = getIndex(name);
  if (i>=0) values[i] = value;
}

//set the label for a parameter
void AsyncWebConfig::setLabel(const char * name, const char* label){
  int16_t i = getIndex(name);
  if (i>=0) strlcpy(_description[i].label,label,LABELLENGTH);
}

//remove all options
void AsyncWebConfig::clearOptions(uint8_t index){
  if (index < _count) _description[index].optionCnt = 0;
}

void AsyncWebConfig::clearOptions(const char * name){
  int16_t i = getIndex(name);
  if (i >= 0) clearOptions(i);
}

//add a new option
void AsyncWebConfig::addOption(uint8_t index, String option){
  addOption(index,option,option);
}

void AsyncWebConfig::addOption(uint8_t index, String option, String label){
  if (index < _count) {
    if (_description[index].optionCnt < MAXOPTIONS) {
      _description[index].options[_description[index].optionCnt]=option;
      _description[index].labels[_description[index].optionCnt]=label;
      _description[index].optionCnt++;
    }
  }
}

//modify an option
void AsyncWebConfig::setOption(uint8_t index, uint8_t option_index, String option, String label){
  if (index < _count) {
    if (option_index < _description[index].optionCnt) {
      _description[index].options[option_index] = option;
      _description[index].labels[option_index] = label;
    }
  }

}

void AsyncWebConfig::setOption(char * name, uint8_t option_index, String option, String label){
  int16_t i = getIndex(name);
  if (i >= 0) setOption(i,option_index,option,label);
}

//get the options count
uint8_t AsyncWebConfig::getOptionCount(uint8_t index){
  if (index < _count) {
    return _description[index].optionCnt;
  } else {
    return 0;
  }
}

uint8_t AsyncWebConfig::getOptionCount(char * name){
  int16_t i = getIndex(name);
  if (i >= 0) {
    return getOptionCount(i);
  } else {
    return 0;
  }

}

//set form type to doen cancel
void AsyncWebConfig::setButtons(uint8_t buttons){
  _buttons = buttons;
}
//register onSave callback
void AsyncWebConfig::registerOnSave(void (*callback)(String results)){
  _onSave = callback;
}
//register onSave callback
void AsyncWebConfig::registerOnDone(void (*callback)(String results)){
  _onDone = callback;
}
//register onSave callback
void AsyncWebConfig::registerOnCancel(void (*callback)()){
  _onCancel = callback;
}
//register onDelete callback
void AsyncWebConfig::registerOnDelete(void (*callback)(String name)){
  _onDelete = callback;
}
