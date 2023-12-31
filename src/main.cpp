#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

WiFiMulti wifiMulti; 
WiFiClient client;

AsyncWebServer GWSERVER(4001);

#define LOGIN_URL     "http://78.157.34.36:4001/api/users/login" 
#define BUILDING_URL  "http://78.157.34.36:4001/api/buildings"
#define ROOM_URL      "http://78.157.34.36:4001/api/rooms?buildingId="
#define ADD_URL       "http://78.157.34.36:4001/api/addresses?buildingId="

#define LOGIN_USER    "root"
#define LOGIN_PASS    "123"
#define ROOM_ID       "6565064afabf334123776f06"
String  BUILDING_ID=  "6565061afabf334123776ecc";

#define LEDPIN BUILTIN_LED

DynamicJsonDocument ReadbleAdds(2000);      //FROM APP        //{"roomID" : {"add" : {"id" : "12456879", "format" : "1Bit", "type" : "temp" } } }
DynamicJsonDocument WritableAdds(2000);     //FROM APP        //{"1/1/2" : "format"}
DynamicJsonDocument APPadd_MAP_Actadd(500); //FROM GW CONFIG  //{"1/1/2": IO(4)}

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

String Token = "";
//---------------------------v-----------------
//WIFI & Connections Functions
//---------------------------*-----------------
void WifiConnect(String SSID , String PASS , int LedPin)
{
  if(wifiMulti.run() != WL_CONNECTED) {
  Serial.println("WiFi not connected!");
  digitalWrite(LedPin , !digitalRead(LedPin));
  delay(500);
  }
  digitalWrite(LedPin , LOW);
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}
//---------------------------v-----------------
//APP Functions
//---------------------------*-----------------
String PostReq(String URL ,String Reqdata,  String token)
{
  WiFiClient client;
  HTTPClient http;
  String response = "";
  
  http.begin(client, URL.c_str());  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", token);

  int httpResponseCode = http.POST(Reqdata);
  if (httpResponseCode > 0) 
  {
    Serial.print("HTTP POST Response code: ");
    Serial.println(httpResponseCode);
    response = http.getString();
  } 
  else 
  {
    Serial.print("[POST] Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return response;
}

String GetReq(String URL , String token)
{
  WiFiClient client;
  HTTPClient http;
  String payload = "";
  http.begin(client, URL);
  http.addHeader("Authorization", token);

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    payload = http.getString();
  } else 
  {
    Serial.print("[GET] Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

bool Login(String User , String Pass)
{
  DynamicJsonDocument LoginInput(200);
  LoginInput["username"] = User;
  LoginInput["password"] = Pass;
  String LoginReq = "";
  serializeJson(LoginInput, LoginReq);
  String LoginRes = PostReq(LOGIN_URL ,LoginReq, Token);

  StaticJsonDocument<16> filter;
  filter["token"] = true;
  DynamicJsonDocument ResObj(256);
  DeserializationError error = deserializeJson(ResObj, LoginRes,DeserializationOption::Filter(filter));
  Serial.println();
  serializeJsonPretty(ResObj, Serial);
  Serial.println();
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return 0;
  }

  if(ResObj.size() == 0)
  {
    Serial.println("Result is empty");
    return 0;
  }
  Token = ResObj["token"].as<String>();
  return 1;
}

void Biulding(String URL)
{
  String BuildingRes = GetReq(URL, Token);
  StaticJsonDocument<64> filter;
  JsonObject filter_buildings_0 = filter["buildings"].createNestedObject();
  filter_buildings_0["_id"] = true;
  filter_buildings_0["name"] = true;

  StaticJsonDocument<1000> BuildingResObj;
  DeserializationError error;
  do
  {
    DeserializationError error = deserializeJson(BuildingResObj, BuildingRes, DeserializationOption::Filter(filter));
    Serial.println();
    serializeJsonPretty(BuildingResObj, Serial);
    Serial.println();
     if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    delay(300);
    }

  } while (error);
}

String GetConf(String URL, String BuildingID, int JsonCapacity , StaticJsonDocument<300> Filter)
{
  DeserializationError error;
  DynamicJsonDocument ConfResObj(JsonCapacity);
  do
  {
    ConfResObj.clear();
    String ConfRes = GetReq(URL + BuildingID, Token);
    DeserializationError error = deserializeJson(ConfResObj, ConfRes, DeserializationOption::Filter(Filter));
    Serial.println();
    serializeJsonPretty(ConfResObj, Serial);
    Serial.println();
     if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    delay(300);
    }

  } while (error);

  String FilteredRes = "";
  serializeJson(ConfResObj, FilteredRes);

  return FilteredRes;
}

String CalReadWriteAdss(String RoomID , String RoomsConfigs, int RoomJsonSize, String RoomsAddConfigs, int AddsJsonSize)
{
  //RoomsConfigs --> {"_id": "6549e908ea75d4a5df95ad2a", "addresses": ["6549e8bbea75d4a5df95ad11", "6549e937ea75d4a5df95ad3c" ] }
  //RoomsAddConfigs -->  {"_id": "6549e937ea75d4a5df95ad3c", "values": [],"format": "1 Byte (Unsigned)", "writeTo": "", "readFrom": "0/0/3","rangeType": "temprature"}
  DynamicJsonDocument RoomsConfJson(RoomJsonSize);
  DeserializationError error = deserializeJson(RoomsConfJson, RoomsConfigs);
  DynamicJsonDocument AddsConfJson(AddsJsonSize);
  error = deserializeJson(AddsConfJson, RoomsAddConfigs);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return "NULL";
  }
  //Cleare Lists
  ReadbleAdds.clear();
  WritableAdds.clear();
  //check all addresses in rooms
  for(int roomnum = 0 ; roomnum < RoomsConfJson["rooms"].size() ; roomnum++)
  {
    String room_ID = RoomsConfJson["rooms"][roomnum]["_id"];
    if(RoomID == room_ID) // room founded
    {
      JsonObject roomradd_format = ReadbleAdds.createNestedObject(room_ID);

      for(int addnum = 0; addnum < RoomsConfJson["rooms"][roomnum]["addresses"].size() ; addnum++)
      {
        String RoomsAdds = RoomsConfJson["rooms"][roomnum]["addresses"][addnum];
        for(int roomaddsnum = 0; roomaddsnum < AddsConfJson["addresses"].size(); roomaddsnum++)
        {
          String addID = AddsConfJson["addresses"][roomaddsnum]["_id"];
          if(RoomsAdds == addID)
          {
            Serial.println(addID);
    
            if(AddsConfJson["addresses"][roomaddsnum]["values"].size() == 0)
            {
              Serial.println("Founded!");
              if (AddsConfJson["addresses"][roomaddsnum]["readFrom"].as<String>() != "")
              {
                JsonObject addID_format = roomradd_format.createNestedObject(AddsConfJson["addresses"][roomaddsnum]["readFrom"].as<String>());
                addID_format["format"] = AddsConfJson["addresses"][roomaddsnum]["format"].as<String>();
                addID_format["id"]     = AddsConfJson["addresses"][roomaddsnum]["_id"].as<String>();
                addID_format["type"]   = AddsConfJson["addresses"][roomaddsnum]["rangeType"].as<String>();
              }
              if (AddsConfJson["addresses"][roomaddsnum]["writeTo"].as<String>() != "")
                WritableAdds[AddsConfJson["addresses"][roomaddsnum]["writeTo"].as<String>()] = AddsConfJson["addresses"][roomaddsnum]["format"].as<String>();
              // roomradd_format["add"] = AddsConfJson["addresses"][roomaddsnum]["readFrom"];
              // roomradd_format["format"] = AddsConfJson["addresses"][roomaddsnum]["format"];
            }
            else
            {
              Serial.print("Founded List! seize: ");
              Serial.println(AddsConfJson["addresses"][roomaddsnum]["values"].size());
              for(int ValueNum = 0; ValueNum < AddsConfJson["addresses"][roomaddsnum]["values"].size(); ValueNum++)
              {
                if (AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["read"].as<String>() != "")
                {
                  JsonObject addID_format = roomradd_format.createNestedObject(AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["read"].as<String>());
                  addID_format["format"] = AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["readFormat"].as<String>();
                  addID_format["id"]     = AddsConfJson["addresses"][roomaddsnum]["_id"].as<String>();
                  addID_format["type"]   = AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["readValueRangeType"].as<String>();
                }
                if (AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["write"].as<String>() != "")
                  WritableAdds[AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["write"].as<String>()] = AddsConfJson["addresses"][roomaddsnum]["values"][ValueNum]["writeFormat"].as<String>();
              }
            }
          }
        }
      }
    }
  }
  Serial.println("+++++READ+++++++");
  serializeJsonPretty(ReadbleAdds,Serial);
  Serial.println("\n===============\n");
  Serial.println("+++++Write+++++++");
  serializeJsonPretty(WritableAdds,Serial);
  Serial.println("\n===============");
  return "Null";
}

void GetTotalConf(String BUILDINGID)
{
  //Wait until login success
  while(!Login(LOGIN_USER,LOGIN_PASS)){delay(500);Serial.println("Attempting to LOGIN...");}
  Serial.println("\n---------------\nTry GET Buildings Confing...");
  Biulding(BUILDING_URL);
  // Wait until Get rooms conf
  StaticJsonDocument<64> roomfilter;
  JsonObject filter_rooms_0 = roomfilter["rooms"].createNestedObject();
  filter_rooms_0["_id"] = true;
  filter_rooms_0["addresses"] = true;
  String TotalRoomsConf = "";
  while(TotalRoomsConf == ""){Serial.println("\n---------------\nTry GET Rooms Confing...");TotalRoomsConf = GetConf(ROOM_URL, BUILDINGID, 15000, roomfilter);delay(500);}
  // //Wait until Get addresses conf
  StaticJsonDocument<240> Addfilter;
  JsonObject filter_addresses_0 = Addfilter["addresses"].createNestedObject();
  filter_addresses_0["_id"] = true;
  filter_addresses_0["format"] = true;
  filter_addresses_0["writeTo"] = true;
  filter_addresses_0["readFrom"] = true;
  filter_addresses_0["rangeType"] = true;
  JsonObject filter_addresses_0_values_0 = filter_addresses_0["values"].createNestedObject();
  filter_addresses_0_values_0["read"] = true;
  filter_addresses_0_values_0["readFormat"] = true;
  filter_addresses_0_values_0["readValueRangeType"] = true;
  filter_addresses_0_values_0["write"] = true;
  filter_addresses_0_values_0["writeFormat"] = true;
  filter_addresses_0_values_0["writeValueRangeType"] = true;
  String TotalAddConf = "";
  while(TotalAddConf == ""){Serial.println("\n---------------\nTry GET Adds Confing...");TotalAddConf = GetConf(ADD_URL, BUILDINGID, 15000, Addfilter);delay(500);}
  
  Serial.println("\n========Create Readble Adds with Configs========\n");
  CalReadWriteAdss(ROOM_ID, TotalRoomsConf, 15000, TotalAddConf, 15000);
}

String WriteReq(int value, String Add)
{
  String Res = "";
  DynamicJsonDocument ResStatus(150); 
  ResStatus["addressId"] = Add;

  if(WritableAdds.containsKey(Add) && APPadd_MAP_Actadd.containsKey(Add))
  {
    digitalWrite(APPadd_MAP_Actadd[Add].as<int>(), value);
    ResStatus["message"] = "Write Status: Success";
    ResStatus["status"]   = 200;
  }
  else
  {
    ResStatus["status"]   = 404;
    ResStatus["message"]  = "Add " + String(Add) + " is not defined!";
  }
  serializeJsonPretty(ResStatus, Res);
  Serial.println(Res);
  return Res;
}

String ReadReq(String RoomReadID)
{
  String Res = "";
  DynamicJsonDocument ResStatus(500); 
  ResStatus["status"] = 200;

  // Add the values to the JSON document
  JsonArray resultArray = ResStatus.createNestedArray("Result");

  bool RoomFounded = false;


  for (JsonPair roID : ReadbleAdds.as<JsonObject>())//{"roomID" : {"0/0/1" : {"id" : "12456879", "format" : "1Bit", "type" : "temp" } } }
  {
    if(strcmp(roID.key().c_str(), RoomReadID.c_str()) == 0) //RoomID Founded
    {
      Serial.println("Room Founded!");
      RoomFounded = true;
      for (JsonPair reADD : roID.value().as<JsonObject>())// {"0/0/1" : {"id" : "12456879", "format" : "1Bit", "type" : "temp" }}
      {
        JsonObject addConf = reADD.value().as<JsonObject>();
        String readADD = reADD.key().c_str();
        if(APPadd_MAP_Actadd.containsKey(readADD))
        {
          JsonObject resultObject = resultArray.createNestedObject();
          resultObject["Id"] = addConf["id"];
          resultObject["MultiValue"] = nullptr;
          resultObject["Value"] = digitalRead(APPadd_MAP_Actadd[readADD].as<int>());
        }
      }
    }
  }
  if(RoomFounded)
  {
    Serial.println("=====Read Res");
    serializeJsonPretty(ResStatus, Serial);
    Serial.println();
    serializeJson(ResStatus, Res);
  }
  else
  {
    Res = "{\"message\": \"RoomId not found\",\"status\": 404}";
    Serial.println("=====Read Res");
    Serial.println(Res);
  }
  return Res;
}


void setup() {
  Serial.begin(115200);
  APPadd_MAP_Actadd["0/0/1"] = LEDPIN;
  APPadd_MAP_Actadd["0/0/2"] = LEDPIN;
  APPadd_MAP_Actadd["0/0/3"] = LEDPIN;
  APPadd_MAP_Actadd["0/0/4"] = LEDPIN;

  pinMode(LEDPIN, OUTPUT);
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP("IGH-Wifi", "Konect210");
  wifiMulti.addAP("MobinNet20", "K6YJScyY");
  wifiMulti.addAP("Irancell-TF-i60-B6A7_1", "@tm@1425#@tm@");
  Serial.println("Connecting Wifi...");
  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  while(!MDNS.begin("konectgw"))
  {
     Serial.println("Starting mDNS...");
     delay(1000);
  }
  Serial.println("MDNS started");
  delay(1000);
  //------------ GET CONFIGS FROM SERVER -----------------
  GetTotalConf(BUILDING_ID);

  GWSERVER.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });
  GWSERVER.onRequestBody(
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if ((request->url() == "/api/gateway/write") &&
                (request->method() == HTTP_POST))
            {
                const size_t        JSON_DOC_SIZE   = 512U;
                DynamicJsonDocument jsonDoc(JSON_DOC_SIZE);
                
                if (DeserializationError::Ok == deserializeJson(jsonDoc, (const char*)data))
                {
                    JsonObject obj = jsonDoc.as<JsonObject>();
                    Serial.print("Write Req: ");
                    serializeJsonPretty(jsonDoc, Serial);
                    Serial.println();
                    
                    if (obj.containsKey("value") && obj.containsKey("addressId"))
                    {
                      Serial.println("Json is valid");
                      int WriteValue  = obj["value"].as<int>();
                      String WriteAdd = obj["addressId"].as<String>();
                      String Response = WriteReq(WriteValue, WriteAdd);
                      request->send(200, "application/json", Response);
                    }
                    else
                      Serial.println("Invalid JSON");
                      request->send(200, "application/json", "{ \"status\": 404, \"massage\" : \"Invalid JSON\" , \"addressId\":\"-1/-1/-1\"}");
                }
            }

            else if ((request->url() == "/api/gateway/read") &&
                (request->method() == HTTP_POST))
            {
                const size_t        JSON_DOC_SIZE   = 512U;
                DynamicJsonDocument jsonDoc(JSON_DOC_SIZE);
                
                if (DeserializationError::Ok == deserializeJson(jsonDoc, (const char*)data))
                {
                    JsonObject obj = jsonDoc.as<JsonObject>();
                    Serial.print("Read Req: ");
                    serializeJsonPretty(jsonDoc, Serial);
                    Serial.println();
                    
                    if (obj.containsKey("RoomId"))
                    {
                      Serial.println("Json is valid");
                      String RoomId = obj["RoomId"].as<String>();
                      String Response = ReadReq(RoomId);
                      request->send(200, "application/json", Response);
                    }
                    else
                      Serial.println("Invalid JSON");
                      request->send(200, "application/json", "{ \"status\": 404, \"massage\" : \"Invalid JSON\" }");
                }
            }

            else if ((request->url() == "/api/gateway/update") &&
                (request->method() == HTTP_POST))
            {
                const size_t        JSON_DOC_SIZE   = 512U;
                DynamicJsonDocument jsonDoc(JSON_DOC_SIZE);
                
                if (DeserializationError::Ok == deserializeJson(jsonDoc, (const char*)data))
                {
                    JsonObject obj = jsonDoc.as<JsonObject>();
                    Serial.print("Write Req: ");
                    serializeJsonPretty(jsonDoc, Serial);
                    Serial.println();
                    
                    if (obj.containsKey("buildingId"))
                    {
                      Serial.println("Json is valid");
                      BUILDING_ID = obj["buildingId"].as<String>();
                      GetTotalConf(BUILDING_ID);
                      request->send(200, "application/json", "{\"status\" : 200 , \"message\" : \"Update Status: Success\"}" );
                    }
                    else
                      Serial.println("Invalid JSON");
                      request->send(200, "application/json", "{ \"status\": 404, \"massage\" : \"Invalid JSON\" }");
                }
            }
        }
    );

  GWSERVER.begin();
}

void loop() {
  sleep(100);
}
