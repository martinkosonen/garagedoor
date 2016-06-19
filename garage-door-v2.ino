/*
  Basic ESP8266 MQTT example

  This sketch demonstrates the capabilities of the pubsub library in combination
  with the ESP8266 board/library.

  It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

  To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define swClosed D1
#define swOpen D2
#define doorTrigger D5
#define wpsReset D6

int doorStatus;                         // Initialize the door status integer variable (0, 1, 2 or 3)

// Update these with values suitable for your network.

const char* mqtt_server = "10.0.1.100"; // Server IP for MQTT
const char devname[32] = "dev0";        // Device name for MQTT (needs to be unique to other devices)

unsigned long debounceDelay = 100;      // Delay for the switch debouncing circuit
unsigned long lastDebounce = 0;         // Initialize the Last Time Debounced  variable
unsigned long doorUpdate = 10000;
unsigned long lastUpdate;
//unsigned long lastDoorCheck = 0;        // Initilize the door check timing variable
//unsigned long doorCheckDelay = 15000;   // Delay before checking 
//int maxDoorChecks = 3;                  // Number of retries before cancelling the door attempt
//int doorCheck;                          // Initialize current door check number

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int uptime = 0;
char message_buff[100];

// THis routine checks the door status. 0 = Closed, 1 = Open, 2 = Transit/Paused, 3 is implausible
int checkDoor() {
  if (digitalRead(swClosed) == LOW && digitalRead(swOpen) == HIGH) {
    return 0;
  }
  else if (digitalRead(swClosed) == HIGH && digitalRead(swOpen) == LOW) {
    return 1;
  }
  else if (digitalRead(swClosed) == HIGH && digitalRead(swOpen) == HIGH) {
    return 2;
  }
  else
  {
    return 3;
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
  //  Serial.print("Message arrived [");
  //  Serial.print(topic);
  //  Serial.print("] ");
  //  for (int i = 0; i < length; i++) {
  //    Serial.print((char)payload[i]);
  //  }
  //  Serial.println();

  // Switch on the LED if an 1 was received as first character
  //  if ((char)payload[0] == '1') {
  //    digitalWrite(D0, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is acive low on the ESP-01)
  //  } else {
  //    digitalWrite(D0, HIGH);  // Turn the LED off by making the voltage HIGH
  //  }
  int i = 0;                                  // Convert incoming payload to a string
  for (i = 0; i < length; i++) {
    message_buff[i] = payload [i];
  }
  message_buff[i] = '\0';
  String msgString = String(message_buff);
  String msgTopic = String(topic);

  Serial.println("Topic: " + msgTopic);
  Serial.println("Payload: " + msgString);
  if (msgString.equals("debug") && msgTopic.equals("debug")) {            // Check the incoming string to match "debug" on topic "debug"
    debug();                                  // Run the debug subroutine
  }
  else if (msgString.equals("debug") && msgTopic.equals(String(devname))){     // Check incoming string to match "debug" on the device name topic
    debug();
  }
  else if (msgString.equals("resetwps") && msgTopic.equals(String(devname))){
    resetWPS();
  }
  else if (msgString.equals("doorTrigger") && msgTopic.equals("home/garage")){
    digitalWrite(doorTrigger, HIGH);
    digitalWrite(D0, LOW);
    delay(750);
    digitalWrite(doorTrigger,LOW);
    digitalWrite(D0, HIGH);
  }


}

void setup() {
  pinMode(D0, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  digitalWrite(D0, HIGH);
  pinMode(swClosed, INPUT_PULLUP);  // Door Closed Switch
  pinMode(swOpen, INPUT_PULLUP);    // Door Opened Switch
  pinMode(wpsReset, INPUT_PULLUP);  // WPS Reset Pushbutton (runs WiFi.Disconnect() Routine)
  pinMode(doorTrigger, OUTPUT);     // Relay in OR config with pushbutton to trigger the door
  digitalWrite(doorTrigger, LOW);   // Initialize the door relay as LOW
  Serial.begin(115200);
  // WPS works in STA (Station mode) only.
  WiFi.mode(WIFI_STA);
  delay(1000);
  digitalWrite(D0, LOW);
  // Called to check if SSID and password has already been stored by previous WPS call.
  // The SSID and password are stored in flash memory and will survive a full power cycle.
  // Calling ("",""), i.e. with blank string parameters, appears to use these stored values.
  WiFi.begin("", "");
  //Reset
  //  WiFi.disconnect(); // Un-Comment this line to remove the existing Wifi configuration parameters and reinitiate the WPS system.  This will be integrated into a pushbutton reset eventually.....
  //  WiFi.begin("", "");
  // Long delay required especially soon after power on.
  Serial.println("\nConnecting to Wifi ...");
  delay(4000);
  // Check if WiFi is already connected and if not, begin the WPS process.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nAttempting WPS initialization ...");
    WiFi.beginWPSConfig();
    // Another long delay required.
    delay(3000);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
      Serial.println(WiFi.localIP());
      Serial.println(WiFi.SSID());
      Serial.println(WiFi.macAddress());
      Serial.println("\n");
    }
    else {
      Serial.println("Connection failed!");
    }
  }
  else {
    Serial.println("\nWPS connection already established.");
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.SSID());
    Serial.println(WiFi.macAddress());

  }
  client.setServer(mqtt_server, 1883);  // MQTT Server/Port Init
  client.setCallback(callback);         // MQTT Callback Routine to Listen
  digitalWrite(D0, HIGH);               //
}



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(devname)) {
      Serial.println("connected");
      client.subscribe("home/garage");
      client.loop();
      client.subscribe("debug");
      client.loop();
      client.subscribe(devname);
      debug();
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop(); //required to run the MQTT server

//  long now = millis();
  if (millis() - lastMsg > 60000) {
    lastMsg = millis();
    ++uptime;
    //    client.publish("outTopic", msg);
  }
  if (digitalRead(wpsReset) == LOW) {
    resetWPS();
  }
  if((millis() - lastDebounce) > debounceDelay){        // Reed switches with movement can be bouncy so we debounce
    if(doorStatus != checkDoor()) {                     // If the current door position is not what the door position was..
      updateDoor();                                     
      doorStatus = checkDoor();                         // Make the current door status what it is so it can monitor for changes
      lastDebounce = millis();                          // Reset the debounce timer
    }
  }
  if((millis() - lastUpdate) > lastUpdate){
    updateDoor();
    lastUpdate = millis();
  }
}

void updateDoor(){
    char doorStatus_buff[3];
    String(checkDoor()).toCharArray(doorStatus_buff, String(checkDoor()).length()+1);
    client.publish("home/garage/doorstatus", doorStatus_buff);  // Publish the new position
}
//void doorControl(int doorCommand) {
//  if(doorCommand = checkDoor()){
//    return;
//  }
//  else if(doorCommand != checkDoor()){
//    Serial.println("Toggling Door")
//    digitalWrite(doorTrigger, HIGH);
//    delay(250);
//    digitalWrite(doorTrigger, LOW);
    
//  }
//}

//void postCommandCheck(){
  // CHeck if the recent (30 sec?) command was performed correct.  Otherwise, retoggle the door and check it again.  After 3 attempts to correct, error out)
  // Not yet implemented.  Will be a dumb switch for now.
//}

void resetWPS() {
  Serial.println("Resetting Wifi Settings");
  delay(500);
 // WiFi.disconnect();
//  WiFi.begin("", "");
  debug();
  delay(1000);          //Delay... Maybe not needed??
}

void debug() {            // This runs when the device receives a debug MQTT message.
  char debug_buff[100];
  String ssid = String(WiFi.SSID());              // Gather the data to send
  String ipaddress = WiFi.localIP().toString();
  String rssi = String(WiFi.RSSI());
  String mac = String(WiFi.macAddress());

  // Create JSON string from the data
  String pubString = "{\"" + String(devname) + "\":{\"ssid\": \"" + String(ssid) + "\", \"ipaddress\": \"" + String(ipaddress) + "\", \"rssi\": " + String(rssi) + ", \"uptime\": " + String(uptime) + "}}";
  pubString.toCharArray(debug_buff, pubString.length() + 1);
  client.publish("debug", debug_buff);            // Publish JSON string to the "debug" topic.
}

