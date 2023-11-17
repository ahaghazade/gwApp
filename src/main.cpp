#include <ArduinoJson.h>

void setup() {
  // Initialize Serial port
  Serial.begin(115200);
  while (!Serial) continue;

  // StaticJsonDocument<200> doc;
  DynamicJsonDocument  doc(200);

  // Add values in the document
  doc["sensor"] = "gps";
  doc["time"] = 1351824120;
  doc["_id"] = "asd456sdqwer";

  // Add an array.
  JsonArray data = doc.createNestedArray("values");
  data.add(48.756080);
  data.add(2.302038);

  // Generate the minified JSON and send it to the Serial port.
  //
  serializeJson(doc, Serial);
  // The above line prints:
  // {"sensor":"gps","time":1351824120,"data":[48.756080,2.302038]}

  Serial.println();
  serializeJsonPretty(doc, Serial);
  // The above line prints:
  // {
  //   "sensor": "gps",
  //   "time": 1351824120,
  //   "values": [
  //     48.756080,
  //     2.302038
  //   ]
  // }
  Serial.println();
  int Value1 = doc["values"][0];//48
  Serial.println(Value1);
  
}

void loop() {
  // not used in this example
}