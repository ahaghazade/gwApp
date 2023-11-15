#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

const char* WifiSsid = "MobinNet20";
const char* WifiPassword = "K6YJScyY";


#define LOGIN_URL     "http://185.110.190.226:4001/api/users/login"
#define BUILDING_URL  "http://185.110.190.226:4001/api/buildings"
#define ROOM_URL      "http://185.110.190.226:4001/api/rooms?buildingId="
#define ADD_URL       "http://185.110.190.226:4001/api/addresses?buildingId="

#define LOGIN_USER    "root"
#define LOGIN_PASS    "123"
#define BUILDING_ID   "6542276fea75d4a5df95a7d0"
#define LEDPIN BUILTIN_LED

JSONVar RoomsConf;
JSONVar BuildingAddsConf;
JSONVar RoomsAdd; // room all addresses

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

String Token = "";
//---------------------------v-----------------
//WIFI & Connections Functions
//---------------------------*-----------------
void WifiConnect(String SSID , String PASS , int LedPin)
{
  WiFi.begin(SSID, PASS);
    Serial.println("Connecting");
    while(WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      digitalWrite(LedPin , !digitalRead(LedPin));
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
  JSONVar LoginInput;
  LoginInput["username"] = User;
  LoginInput["password"] = Pass;
  
  String LoginRes = PostReq(LOGIN_URL ,JSON.stringify(LoginInput), Token);
  Serial.println(LoginRes);
  JSONVar myObject = JSON.parse(LoginRes);
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return 0;
  }
  Token = myObject["token"];
  return 1;
}

void Biulding(String URL)
{
  String BuildingRes = GetReq(URL, Token);
  Serial.println(BuildingRes);
  JSONVar myObject = JSON.parse(BuildingRes);
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }
}

String GetConf(String URL, String BuildingID)
{
  String RoomConfRes = GetReq(URL + BuildingID, Token);
  JSONVar myObject = JSON.parse(RoomConfRes);
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return "";
  }
  return RoomConfRes;
}

void PrintJson(JSONVar myObject) {
  Serial.println("\n{");
  for (int x = 0; x < myObject.keys().length(); x++) {
    if ((JSON.typeof(myObject[myObject.keys()[x]])).equals("object")) {
      Serial.print(myObject.keys()[x]);
      Serial.println(" : ");
      PrintJson(myObject[myObject.keys()[x]]);
    }
    else {
      Serial.print(myObject.keys()[x]);
      Serial.print(" : ");
      Serial.print(myObject[myObject.keys()[x]]);
      Serial.println(",");
    }
  }
  Serial.println("}\n");
}

void RoomsAdds(JSONVar Room_Conf)
{
  // iterate on room
  for (int room = 0; room < Room_Conf["rooms"].length(); room++) 
  {
    JSONVar add;
    //iterate on rooms addresses
    for (int addCount = 0 ; addCount < Room_Conf["rooms"][room]["addresses"].length() ; addCount++)
    {
      Serial.println(Room_Conf["rooms"][room]["addresses"][addCount]);
      add[addCount] = String(JSON.stringify(Room_Conf["rooms"][room]["addresses"][addCount]));
    }
    Serial.println(add);
    Serial.println("/*/*/*/*/*/*/*/*/");
    Serial.println(Room_Conf["rooms"][room]["_id"]);
    RoomsAdd[Room_Conf["rooms"][room]["_id"]] = add;
    Serial.println(RoomsAdd);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  WifiConnect(WifiSsid, WifiPassword , LEDPIN);
  delay(1000);
  //------------ GET CONFIGS FROM SERVER -----------------
  //Wait until login success
  while(!Login(LOGIN_USER,LOGIN_PASS)){delay(500);Serial.println("Attempting to LOGIN...");}
  Serial.println("\n---------------\nTry GET Buildings Confing...");
  Biulding(BUILDING_URL);
  String TotalRoomsConf = "";
  //Wait until Get rooms conf
  while(TotalRoomsConf == ""){Serial.println("\n---------------\nTry GET Rooms Confing...");TotalRoomsConf = GetConf(ROOM_URL, BUILDING_ID);delay(500);}
  RoomsConf = JSON.parse(TotalRoomsConf);
  Serial.println("+++++++++++++++++++");
  RoomsAdds(RoomsConf);
  // PrintJson(RoomsConf);
  //Wait until Get addresses conf
  String TotalAddConf = "";
  while(TotalAddConf == ""){Serial.println("\n---------------\nTry GET Adds Confing...");TotalAddConf = GetConf(ADD_URL, BUILDING_ID);delay(500);}
  BuildingAddsConf = JSON.parse(TotalAddConf);
}

void loop() {
  sleep(100);
}