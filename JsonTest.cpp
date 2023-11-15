#include <Arduino_JSON.h>

void objRec(JSONVar myObject) {
  Serial.println("{");
  for (int x = 0; x < myObject.keys().length(); x++) {
    if ((JSON.typeof(myObject[myObject.keys()[x]])).equals("object")) {
      Serial.print(myObject.keys()[x]);
      Serial.println(" : ");
      objRec(myObject[myObject.keys()[x]]);
    }
    else {
      Serial.print(myObject.keys()[x]);
      Serial.print(" : ");
      Serial.print(myObject[myObject.keys()[x]]);
      Serial.println(",");
    }
  }
  Serial.println("}");
}

void valueExtractor() {

  Serial.println("object");
  Serial.println("======");
  JSONVar myObject;
  JSONVar add;
  add[0] = "123456";
  add[1] = "123456";
  // Making a JSON Object
  myObject["foo"] = "bar";
  myObject["blah"]["abc"] = add;
  myObject["blah"]["efg"] = "pod";
  myObject["blah"]["cde"]["pan1"] = "King";
  myObject["blah"]["cde"]["pan2"] = 3.14;
  myObject["jok"]["hij"] = "bar";

  Serial.println(myObject);
  Serial.println();
  Serial.println("Extracted Values");
  Serial.println("======");

  objRec(myObject);

}

void setup() {

  Serial.begin(115200);
  while (!Serial);
  valueExtractor();

}

void loop() {
}



