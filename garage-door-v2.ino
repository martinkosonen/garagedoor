

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define swClosed D1
#define swOpen D2
#define doorTrigger D5
#define wpsReset D6
#define debugprintln //                 // All Serial.println commands are called with "debugprintln" This command wipes them before compiling
#define debugprint //                   // Sames as above, but for Serial.print
// #define debugprintln debugprintln  // Uncomment these two lines to enable serial print debugging
// #define debugprint Serial.print

int doorStatus;                         // Initialize the door status integer variable (0, 1, 2 or 3)

// Update these with values suitable for your network.

const char* mqtt_server = "10.0.1.100"; // Server IP for MQTT
const char devname[32] = "dev0";        // Device name for MQTT (needs to be unique to other devices)
unsigned long debounceDelay = 100;      // Delay for the switch debouncing circuit (relatively high because of the magnetic switches
unsigned long lastDebounce = 0;         // Initialize the Last Time Debounced  variable
unsigned long doorUpdate = 10000;
unsigned long lastUpdate;

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

  int i = 0;                                  // Convert incoming MQTT message (payload) to a string
  for (i = 0; i < length; i++) {
    message_buff[i] = payload [i];
  }
  message_buff[i] = '\0';
  String msgString = String(message_buff);
  String msgTopic = String(topic);

  debugprintln("Topic: " + msgTopic);
  debugprintln("Payload: " + msgString);
  
  if (msgString.equals("debug") && msgTopic.equals("debug")) {            // Check the incoming string to match "debug" on topic "debug" (useful for all devices to send the debug messages at the same time)
    debug();                                                              // Run the debug subroutine
  }
  else if (msgString.equals("debug") && msgTopic.equals(String(devname))){      // Check incoming string to match "debug" on the device name topic (debug individual devices)
    debug();
  }
  else if (msgString.equals("resetwps") && msgTopic.equals(String(devname))){   // Using an MQTT client, "resetwps" command on topic of the device name will run
    resetWPS();                                                                 // the routine that wipes the WPS memory
  }
  else if (msgString.equals("doorTrigger") && msgTopic.equals("home/garage")){  // Trigger the door relay if "doorTrigger" comes in on the home/garage topic
    digitalWrite(doorTrigger, HIGH);
    digitalWrite(D0, LOW);
    delay(750);
    digitalWrite(doorTrigger,LOW);
    digitalWrite(D0, HIGH);
  }
}

void setup() {
  pinMode(D0, OUTPUT);              // Initialize the BUILTIN_LED pin as an output
  digitalWrite(D0, HIGH);
  pinMode(swClosed, INPUT_PULLUP);  // Door Closed Switch
  pinMode(swOpen, INPUT_PULLUP);    // Door Opened Switch
  pinMode(wpsReset, INPUT_PULLUP);  // WPS Reset Pushbutton (runs WiFi.Disconnect() Routine)
  pinMode(doorTrigger, OUTPUT);     // Relay wired in OR config with garage door pushbutton to trigger the door
  digitalWrite(doorTrigger, LOW);   // Initialize the door relay as LOW
  Serial.begin(115200);
  // WPS works in (Station mode) only.
  WiFi.mode(WIFI_STA);
  digitalWrite(D0, LOW);            // LED on implying the device is booting up
  
  // The SSID and password are stored in memory and arent affected by power cycles and arduino programming uploads.
  // Calling ("",""), uses previously stored Wifi credentials.
  WiFi.begin("", "");
  
  // Needs a relatively long delay to begin the wifi connection process.  4000ms sounds about right (?)
  debugprintln("\nConnecting to Wifi...");
  delay(4000);
 
  if (WiFi.status() != WL_CONNECTED) {      // Check if WiFi is already connected and if not, begin the WPS process.
    debugprintln("\nAttempting WPS initialization...");
    WiFi.beginWPSConfig();
    delay(3000);                            // Another relatively long delay needed to start WPS.  How about 3000ms
    if (WiFi.status() == WL_CONNECTED) {    // Serial print the connection parameters once it connects. 
      debugprintln("Connected!");
      debugprintln(WiFi.localIP());
      debugprintln(WiFi.SSID());
      debugprintln(WiFi.macAddress());
      debugprintln("\n");
    }
    else {
      debugprintln("Connection failed!");
    }
  }
  else {
    debugprintln("\nWPS connection previously established.");  // If the wifi credentials are already saved in memory, this routine runs and prints wifi info.
    debugprintln(WiFi.localIP());
    debugprintln(WiFi.SSID());
    debugprintln(WiFi.macAddress());

  }
  client.setServer(mqtt_server, 1883);  // MQTT Server/Port Init
  client.setCallback(callback);         // MQTT Callback Routine to Listen
  digitalWrite(D0, HIGH);               // Extinguish the LED now that the device is ready
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(devname)) {
      debugprintln("connected");
      client.subscribe("home/garage");
      client.loop();
      client.subscribe("debug");
      client.loop();
      client.subscribe(devname);
      debug();
      
    } else {
      debugprint("failed, rc=");
      debugprint(client.state());
      debugprintln(" try again in 5 seconds");
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

void resetWPS() {
  debugprintln("Resetting Wifi Settings");
  delay(500);
  WiFi.disconnect();
  WiFi.begin("", "");
  delay(1000);            //Delay... Maybe not needed??
}

void debug() {            // Debug routine is called when requested.  Simply sends out an MQTT message on the "debug" topic with the SSID, IP, RSSI and MAC info.
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

