
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define ssid "University of Washington"   // Wifi network SSID
#define password ""                       // Wifi network password

//bool MIRRORING_RUNNING_DOWNSTAIRS = true;

//-------------------MQTT------------------------
#define MQTT_TOPIC "hcdeiol"
const char* mqtt_server = "broker.hivemq.com";
const char* mqtt_username = "";
const char* mqtt_password = "";
const int mqtt_port = 1883;
#define DEVICE_MQTT_NAME "mirroringHCDEIOL"

WiFiClient espClient;
PubSubClient client(mqtt_server, mqtt_port, espClient);


//#define BRIDGE "Lab Green"
//#define BRIDGE "Lab Blue"
//#define BRIDGE "Lab Red"
#define BRIDGE "Sieg Master"

const int button1Pin = 0;
const int button2Pin = 14;
const int button3Pin = 2;

// Keeps track of the number of times the "happy" button has been pressed.
int countButton1 = 0;
// Keeps track of the number of times the "okay" button has been pressed.
int countButton2 = 0;
// Keeps track of the number of times the "sad" button has been pressed.
int countButton3 = 0;

// Variables for reading pushbutton status.
int button1State = 0;
int button2State = 0;
int button3State = 0;

int debounce[] = {0, 0, 0};
// Keeps track of which emotion(s) should be displayed
int emotion = 0;
int prevEmotion = 0;
unsigned long lastEmotionUpdateTime = 0; //Stop us from sending updates to lights too fast
unsigned long lastSystemResetTime = 0;
unsigned long lastDecrement = 0;
// Keeps track of the highest emotion count
int count = 0;
// Keeps track of the order of the emotion counts from least to greatest
int rank = 0;

String ip; //Hue Bridge IP Address
String api_token; //Hue Bridge Authentication api_token , the username

//---------------------------------------------------------------------------------------------------------------------

/////SETUP_WIFI/////
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");  //get the unique MAC address to use as MQTT client ID, a 'truly' unique ID.
  Serial.println(WiFi.macAddress());  //.macAddress returns a byte array 6 bytes representing the MAC address
}

void setup() {
  if (BRIDGE == "Lab Green") {
    ip = "172.28.219.225"; //Lab Green
    api_token = "lxjNgzhDhd0X-qhgM4lsgakORvWFZPKK70pE0Fja"; //Lab Green
  }
  else if (BRIDGE == "Lab Blue") {
    ip = "172.28.219.235"; //Lab Blue
    api_token = "qn41nLuAOgrvfOAPeNQCYB6qannsoS8uDtyBJtMc"; //Lab Blue
  }
  else if (BRIDGE == "Lab Red") {
    ip = "172.28.219.189"; //Lab Red
    api_token = "Lht3HgITYDN-96UYm5mkJ4CEjKj20d3siYidcSq-"; //Lab Red
  }
  else if (BRIDGE == "Sieg Master") {
    ip = "172.28.219.179"; //Sieg Master
    api_token = "rARKEpLebwXuW01cNVvQbnDEkd2bd56Nj-hpTETB"; //Sieg Master
  }


  Serial.begin(115200);
  WiFi.begin(ssid, password);
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);

  setup_wifi();

  //changeGroup(4, 3, "on", "true", "hue", "40000", "bri", "254", "sat", "100"); //Set lights to a cool white.
  //changeGroup(3, 3, "on", "true", "hue", "40000", "bri", "254", "sat", "100"); //Set lights to a cool white.

}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // put your main code here, to run repeatedly:
  button1();
  button2();
  button3();

  getEmotion();
  unsigned long currentTime = millis();
  //Max update speed = once every 5 seconds
  if(emotion != prevEmotion && (currentTime - lastEmotionUpdateTime > 5000)) {
    visualize();
    prevEmotion = emotion;
    lastEmotionUpdateTime = currentTime;
  }
  if(currentTime - lastDecrement > 60000) {
    if(countButton1 < 3) {
      countButton1 --;
    }
    else if(countButton1 < 10) {
      countButton1 -=2;
    }
    else {
      countButton1 = (9*countButton1) / 10;
    }
    if(countButton2 < 3) {
      countButton2 --;
    }
    else if(countButton2 < 10) {
      countButton2 -=2;
    }
    else {
      countButton2 = (9*countButton2) / 10;
    }
    if(countButton3 < 3) {
      countButton3 --;
    }
    else if(countButton3 < 10) {
      countButton3 -=2;
    }
    else {
      countButton3 = (9*countButton3) / 10;
    }
  }
  /* Total reset every hour
  if(currentTime - lastEmotionUpdateTime > 600000) {
    lastEmotionUpdateTime = currentTime;
    countButton1 = 0;
    countButton2 = 0;
    countButton3 = 0;
  }*/
}


//-------------------------------------------------------METHODS--------------------------------------------------------------------------

// Description : Changes the lights attached to the given group ID to the given parameters with the given transition time.
// Can change up to 4 parameters.
void changeGroup(byte groupNum, byte transitiontime, String parameter, String newValue, String parameter2 = "",
                 String newValue2 = "", String parameter3 = "", String newValue3 = "", String parameter4 = "", String newValue4 = "") {

  String req_string = "http://" + ip + "/api/" + api_token + "/groups/" + groupNum + "/action";
  HTTPClient http;
  http.begin(req_string);

  String put_string = "{\"" + parameter + "\":" + newValue + ", \"transitiontime\": " +
                      transitiontime;
  if (!parameter2.equals("")) put_string += + ", \"" + parameter2 + "\": " + newValue2;
  if (!parameter3.equals("")) put_string += + ", \"" + parameter3 + "\": " + newValue3;
  if (!parameter4.equals("")) put_string += ", \"" + parameter4 + "\" : " + newValue4;
  put_string +=  + "}";

  Serial.println("Attempting PUT: " + put_string + " for GROUP: " + String(groupNum));

  int httpResponseCode = http.PUT(put_string);
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("Response code: " + httpResponseCode);
    Serial.println("Response: " + response);
  } else {
    Serial.print("Error on sending PUT Request: ");
    Serial.println(String(httpResponseCode));
  }
  http.end();
}

// Description : Changes the lights attached to the given light ID to the given parameters with the given transition time.
// Can change up to 4 parameters.
void changeLight(byte lightNum, byte transitiontime, String parameter, String newValue, String parameter2 = "",
                 String newValue2 = "", String parameter3 = "", String newValue3 = "", String parameter4 = "", String newValue4 = "") {

  String req_string = "http://" + ip + "/api/" + api_token + "/lights/" + lightNum + "/state";
  HTTPClient http;
  http.begin(req_string);

  String put_string = "{\"" + parameter + "\":" + newValue + ", \"transitiontime\":" + transitiontime;
  if (!parameter2.equals("")) put_string += + ", \"" + parameter2 + "\": " + newValue2;
  if (!parameter3.equals("")) put_string += ", \"" + parameter3 + "\" : " + newValue3;
  if (!parameter4.equals("")) put_string += ", \"" + parameter4 + "\" : " + newValue4;
  put_string +=  + "}";

  Serial.println("Attempting PUT: " + put_string + " for LIGHT: " + String(lightNum));


  int httpResponseCode = http.PUT(put_string);
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("Response code: " + httpResponseCode);
    Serial.println("Response: " + response);
  } else {
    Serial.println("Error on sending PUT Request: ");
    Serial.println(String(httpResponseCode));
  }
  http.end();
}

//Happy Emotion
void button1() {
  button1State = digitalRead(button1Pin);
  //Serial.println(button1State);
  if (button1State == LOW) {
    debounce[0] = 0;
  }
  if (button1State == HIGH and not debounce[0]) {
    debounce[0] = 1;
    countButton1 += 1;
    //pulse();
  }
}

//Okay Emotion
void button2() {
  button2State = digitalRead(button2Pin);
  //Serial.println(button2State);
  if (button2State == LOW) {
    debounce[1] = 0;
  }
  if (button2State == HIGH and not debounce[1]) {
    debounce[1] = 1;
    countButton2 += 1;
    //pulse();
  }
}

//Sad Emotion
void button3() {
  button3State = digitalRead(button3Pin);
  //Serial.println(button3State);
  if (button3State == LOW) {
    debounce[2] = 0;
  }
  if (button3State == HIGH and not debounce[2]) {
    debounce[2] = 1;
    countButton3 += 1;
    //pulse();
  }
}

//Description: Update the emotion variable to the emotion
//with the greatest button count. If none of the emotion
//buttons have been pressed, emotion should equal 0.
//In order to keep track of emotions that are tied as the
//highest count, we build an integer value with all the
//emotion numbers. For example, if emotion 1 (happy) and
//emotion 2 (okay) were both the highest count, the
//emotion value would build to 21.
void getEmotion() {
  emotion = 0;
  if ((count <= countButton1) && (countButton1 != 0)) { // Emotion 1 is happy.
    count = countButton1;
    emotion = 1;
  }
  if ((count <= countButton2) && (countButton2 != 0)) { // Emotion 2 is okay/neutral.
    count = countButton2;
    if ((emotion == 1) && (countButton2 == countButton1)) {
      emotion += 20;
    } else {
      emotion = 2;
    }
  }
  if ((count <= countButton3) && (countButton3 != 0)) { // Emotion 3 is sad.
    count = countButton3;
    if (((emotion == 1) && (countButton3 == countButton1)) || ((emotion == 2) && (countButton3 == countButton2))) {
      emotion += 30;
    } else if ((emotion == 21) && (countButton3 == countButton1) && (countButton3 == countButton2)) {
      emotion += 300;
    } else {
      emotion = 3;
    }
  }
  if (count == 0) {
    Serial.print("No buttons have been pressed yet.");
    emotion = 0;
  } else {
    Serial.print(count);
    Serial.print(" presses for button: ");
    Serial.println(emotion);
  }
}

//---------------------------------HUB GROUP IDS------------------------------------------------------------------------
//  Master Sieg:
//    Group ID : 1
//    Name : Lower Lobby
//    Lights : {22, 15, 10, 21, 7, 23, 16, 14, 11}
//
//    Group ID : 2
//    Name : Upper Lobby
//    Lights : {18, 20, 12, 25, 26, 5, 8, 19, 13, 24, 9, 17}
//
//    Group ID : 3
//    Name : Alternating Lights Set 1
//    Lights : {22, 10, 7, 16, 11, 18, 12, 26, 8, 13, 9}
//
//    Group ID : 4
//    Name : Alternating Lights Set 2
//    Lights : {15, 21, 23, 14, 20, 25, 5, 19, 24, 17}
//
//    Group ID : 6
//    Name : Stripe Closest to Elevator
//    Lights : {10, 23, 11, 12, 5, 13, 17}
//
//    Group ID : 7
//    Name : Stripe in the Middle
//    Lights : {15, 7, 14, 20, 26, 19, 9}
//
//    Group ID : 8
//    Name : Stripe Near Hall
//    Lights : {22, 21, 16, 18, 25, 8, 24}
//
//    Group ID : 9
//    Name : Outer Box Upper Lobby
//    Lights : {18, 20, 12, 5, 13, 17, 9, 24, 8, 25}
//    Note : if doing a pattern with outer box, the inner lights are 26 and 19
//
//    Group ID : 10
//    Name : Outer Box Lower Lobby
//    Lights : {22, 15, 10, 23, 11, 14, 16, 21}
//    Note : if doing a pattern with outer box, the inner light is 7
//
//-----------------------------------------------------------------------------------------------------------------------

//Description : Visualize the emotion with the highest count.
void visualize() {
  if (emotion == 0) { //Default White : Cool White THIS IS THE MirroringDefault VIZ
    sendPalette("MirroringDefault");
  }
  if (emotion == 1) { //Happy THIS IS THE MirroringHappyGradient VIZ
    sendPalette("MirroringHappyGradient");
  }
  if (emotion == 2) { //Okay : Blue and Yellow Lights THIS IS THE MirroringOkayGradient VIZ
    sendPalette("MirroringOkayGradient");
  }
  if (emotion == 3) { //Sad THIS IS THE MirroringSadGradient VIZ
    sendPalette("MirroringSadGradient");
  }
  if (emotion == 21) { // Happy and Okay are displayed. THIS IS THE MirroringOH VIZ
    sendPalette("MirroringOH");
  }
  if (emotion == 31) { // Sad and Happy are displayed. THIS IS THE MirroringSH VIZ
    sendPalette("MirroringSH");
  }
  if (emotion == 32) { // Sad and Okay are displayed. THIS IS THE MirroringSO VIZ
    sendPalette("MirroringSO");
  }
  if (emotion == 321) { // All emotions are displayed. THIS IS THE MirroringSOH VIZ
    sendPalette("MirroringSOH");
  }
}

//Description : Pulse the lights when a button is pressed. The outer box of
//each lobby will change to a green color and the inner lights to a cool white
//before updating to the new visualization.
//void pulse() {
//  changeGroup(9, 1, "on", "true", "hue", "20000", "bri", "254", "sat", "150"); // Upper Lobby
//  changeGroup(9, 10, "on", "true", "hue", "20000", "bri", "50", "sat", "150"); // dim the brightness
//  changeLight(26, 1, "on", "true", "hue", "40000", "bri", "254", "sat", "100");
//  changeLight(19, 1, "on", "true", "hue", "40000", "bri", "254", "sat", "100");
//  changeGroup(10, 1, "on", "true", "hue", "20000", "bri", "254", "sat", "150"); // Lower Lobby
//  changeGroup(10, 10, "on", "true", "hue", "20000", "bri", "50", "sat", "150"); // dim the brightness
//  changeLight(7, 1, "on", "true", "hue", "40000", "bri", "254", "sat", "100");
//  changeGroup(9, 3, "on", "true", "hue", "20000", "bri", "254", "sat", "150"); // snap brightness back faster
//  changeGroup(10, 3, "on", "true", "hue", "20000", "bri", "254", "sat", "150"); // snap brightness back faster
//  delay(1000); // Hold the lights in this setting briefly before changing visualization again.
//}

//Description: Each hour, we must compare the counts of the lights for the summary visualization.
//Rank the emotions from least to greatest.
//void rank()



////receive MQTT messages
//void subscribeReceive(char* topic, byte* payload, unsigned int length) {
//  // Print the topic
//  Serial.println("MQTT message Topic: " + String(topic) + ", Message: ");
//  for (int i = 0; i < length; i ++)
//  {
//    Serial.print(char(payload[i]));
//  }
//  String plString = String((char *)payload);
//  if(char(payload[0] == 'm')) {
//    if(char(payload[1] == '0')) {
//      MIRRORING_RUNNING_DOWNSTAIRS = false;
//    }else if(char(payload[1] == '1')) {
//      MIRRORING_RUNNING_DOWNSTAIRS = true;
//    }
//  }
//  // Print a newline
//  Serial.println("");
//}

//Connect to MQTT server
void reconnect() {
  while (!client.connected()) { // Loop until we're reconnected
    Serial.println("Attempting MQTT connection...");
    if (client.connect(DEVICE_MQTT_NAME, mqtt_username, mqtt_password)) {  // Attempt to connect
      //client.setCallback(subscribeReceive);
      Serial.println("MQTT connected");
      //client.subscribe(subscribe_top); //Does ttt need to sub to anything?
      //sendState(0);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

//Send Palette to Hourglass to push to lights
void sendPalette(String paletteName) {
  StaticJsonBuffer<512> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["palette"] = paletteName;

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(MQTT_TOPIC, buffer, true);
}
