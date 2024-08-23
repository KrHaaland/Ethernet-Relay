/**************************************
SOME NOTES

FT232:
Use FT_PROG to set bus power to 500mA,
Otherwise it wont give enough power to
supply ethernet module and relays!



/*************************************/

#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <avr/wdt.h>
#include <avr/io.h>

// MAC address for your Ethernet shield
//byte mac[] = { 0x02, 0x79, 0xFB, 0x22, 0x8C, 0x10 };  // First produced module
byte mac[] = { 0x0E, 0x97, 0xBF, 0xCF, 0x2F, 0x95 };  // Second produced module
#define W5500_CS   PIN_PD4
#define W5500_RST  PIN_PD6

//relays
#define RELAY1PIN PIN_PD2
#define RELAY2PIN PIN_PD3

// Initialize the Ethernet server
EthernetServer server(80);

// MQTT Client
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// i2c-frequency
const uint16_t i2cclockfrequency = 400000;
#define EEPROM_I2C_ADDRESS 0x50  // The I2C address for the 24C512 EEPROM

// Global variables
unsigned long previousMillismeasure = 0;
int lastStaterelay1 = LOW;    // Variable to store the last state of the input
int lastStaterelay2 = LOW;    // Variable to store the last state of the input

void(* reboot) (void) = 0; //declare reset function at address 0

void restartMicrocontroller() {
  wdt_enable(WDTO_1S);  // Enable Watchdog Timer with a timeout of 15ms
}

// Function to connect to MQTT server
void connectToMqtt() {
  char mqttServer[64];
  retrieveFromEEPROM2(32, mqttServer, sizeof(mqttServer));
  Serial.print(F("mqtt: "));
  Serial.println(mqttServer);
  mqttClient.setServer(mqttServer, 1883);

  while (!mqttClient.connected()) {
    Serial.println(F("Connecting to MQTT..."));
    if (mqttClient.connect("arduinoClient")) {
      Serial.println(F("Connected to MQTT"));

      // Subscribe to topics
      char mqttTopic[64];
      retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));
      char topic1[70];
      char topic2[70];
      strcpy(topic1, mqttTopic);
      strcat(topic1, "/relay1/set");
      strcpy(topic2, mqttTopic);
      strcat(topic2, "/relay2/set");
      mqttClient.subscribe(topic1);
      mqttClient.subscribe(topic2);

    } 
    else {
      Serial.print(F("Failed, rc="));
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

// MQTT message callback function
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate the payload
  String message = String((char*)payload);

  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  Serial.println(message);

  // Check the topic and payload to control relays
  if (strstr(topic, "/relay1/set")) {
    if (message == "ON" || message == "1") {
      digitalWrite(RELAY1PIN, HIGH);
      Serial.println(F("Relay 1 turned ON"));
    } else if (message == "OFF" || message == "0") {
      digitalWrite(RELAY1PIN, LOW);
      Serial.println(F("Relay 1 turned OFF"));
    }
  } else if (strstr(topic, "/relay2/set")) {
    if (message == "ON" || message == "1") {
      digitalWrite(RELAY2PIN, HIGH);
      Serial.println(F("Relay 2 turned ON"));
    } else if (message == "OFF" || message == "0") {
      digitalWrite(RELAY2PIN, LOW);
      Serial.println(F("Relay 2 turned OFF"));
    }
  }
}

// Function to publish to a specific subtopic
void publishToSubtopic(char* baseTopic, const char* subtopic, const char* message) {
  char fullTopic[75]; // Ensure this buffer is large enough
  strcpy(fullTopic, baseTopic);
  strcat(fullTopic, subtopic); // Directly concatenate the subtopic
  mqttClient.publish(fullTopic, message);
}

void processClientRequest(EthernetClient& client) {
  //Serial.println(freeMemory());
String httpReq = "";
bool gotGet = false;
bool gotHost = false;
String currentLine = "";

while (client.connected()) {
    if (client.available()) {
        char c = client.read();
        currentLine += c;

        // Look for lines that starts with "GET" and "Host" 
        // and keep only them, this in order to save ram

        // Check for the end of the line
        if (c == '\n') {
            // Check if the line contains "GET"
            if (currentLine.startsWith(F("GET"))) {
                gotGet = true;
                httpReq += currentLine;
            }

            // Check if the line contains "Host"
            if (currentLine.startsWith(F("Host"))) {
                gotHost = true;
                httpReq += currentLine;
            }

            // Clear the current line
            currentLine = "";
        String hostValue;
        if (gotGet && gotHost) {
          Serial.println(httpReq);
          // Check for "Host: " header
          // This is used to know if it should redirect back to IP or mDNS-name
          int pos1 = httpReq.indexOf(F("Host: "));
          if (pos1 != -1) {
            int endOfLineIndex = httpReq.indexOf('\n', pos1);
            hostValue = httpReq.substring(pos1 + 6, endOfLineIndex); // +6 to skip past "Host: "
          }
          // Check for "GET: " header
          int pos = httpReq.indexOf(F("GET /?in1="));
          if (pos != -1) {
            pos += 10; // Skip past "GET /?input1="
            // Extract input1
            int nextAmp = httpReq.indexOf('&', pos);
            String input1 = httpReq.substring(pos, nextAmp);

            // Extract input2
            pos = httpReq.indexOf(F("in2="), nextAmp) + 4;
            nextAmp = httpReq.indexOf('&', pos);
            String input2 = httpReq.substring(pos, nextAmp);

            // Extract input3
            pos = httpReq.indexOf(F("in3="), nextAmp) + 4;
            int endPos = httpReq.indexOf('&', pos);
            String input3 = httpReq.substring(pos, endPos);

            // Extract DHCP-selection
            pos = httpReq.indexOf(F("net="), nextAmp) + 4;
            endPos = httpReq.indexOf('&', pos);
            String dhcpselect = httpReq.substring(pos, endPos);

            // Extract input4
            pos = httpReq.indexOf(F("in4="), nextAmp) + 4;
            endPos = httpReq.indexOf('&', pos);
            String input4 = httpReq.substring(pos, endPos);

            // Extract input5
            pos = httpReq.indexOf(F("in5="), nextAmp) + 4;
            endPos = httpReq.indexOf('&', pos);
            String input5 = httpReq.substring(pos, endPos);

            // Extract input6
            pos = httpReq.indexOf(F("in6="), nextAmp) + 4;
            endPos = httpReq.indexOf('&', pos);
            String input6 = httpReq.substring(pos, endPos);

            // Extract input7
            pos = httpReq.indexOf(F("in7="), nextAmp) + 4;
            endPos = httpReq.indexOf(' HTTP/1.1', pos) - 6;
            String input7 = httpReq.substring(pos, endPos);

            // Redirect browser back after pressing "submit"-button
            Serial.print(F("redirecting back to "));
            Serial.println(hostValue);
            client.println(F("HTTP/1.1 302 Found"));
            client.print(F("Location: http://"));
            client.print(hostValue);
            client.println(F("/"));
            client.println(F("Connection: close"));
            client.println();
            // Store in EEPROM, unless input is empty
            // Store MQTT-Server
            if (input1 != NULL){
              input1.replace("%2F", "/");
              storeInEEPROM(32, input1.substring(0, 63)); // Trim the string to the first 32 characters);
              // disconnect MQTT-server in case server-config is updated
              mqttClient.disconnect();
            }
            // Store MQTT-Topic
            if (input2 != NULL){
              input2.replace("%2F", "/");
              storeInEEPROM(96, input2.substring(0, 63)); // Trim the string to the first 32 characters);
              Serial.println(input2.substring(0, 63)); // Trim the string to the first 32 characters););
            }

            // Store update interval)
            if (input3 != NULL){
              unsigned int intinterval = input3.toInt();
              // Break the integer into two bytes
              byte lowByte = intinterval & 0xFF;
              byte highByte = (intinterval >> 8) & 0xFF;
              EEPROM.write(17, lowByte);
              EEPROM.write(18, highByte);
              Serial.println(String(intinterval));
            }          
          
            // Store DHCP-mode
            if (dhcpselect != NULL){
              if ((dhcpselect == "Static") && (EEPROM.read(0) != 1)) {
              EEPROM.write(0, 1);
              Serial.println(F("1 Was stored to EEPROM (Static)"));
              // Reboot to apply new settings
              reboot();
              }
              if ((dhcpselect == F("DHCP")) && (EEPROM.read(0) != 0)) {
              EEPROM.write(0, 0);
              Serial.println(F("0 Was stored to EEPROM (DHCP)"));
              // Reboot to apply new settings
              reboot();
              }              
            } 
            // Store Device-IP
            if (input4 != NULL){
              int ip1, ip2, ip3, ip4;
              char ipCharArray[16];
              input4.toCharArray(ipCharArray, sizeof(ipCharArray));
              sscanf(ipCharArray, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);
              EEPROM.write(1, ip1);
              EEPROM.write(2, ip2);
              EEPROM.write(3, ip3);
              EEPROM.write(4, ip4);
            } 

            // Store Device-subnet
            if (input5 != NULL){
              int ip5, ip6, ip7, ip8;
              char subCharArray[16];
              input5.toCharArray(subCharArray, sizeof(subCharArray));
              sscanf(subCharArray, "%d.%d.%d.%d", &ip5, &ip6, &ip7, &ip8);
              EEPROM.write(5, ip5);
              Serial.print(ip5);
              Serial.print(".");
              EEPROM.write(6, ip6);
              Serial.print(ip6);
              Serial.print(".");              
              EEPROM.write(7, ip7);
              Serial.print(ip7);
              Serial.print(".");
              EEPROM.write(8, ip8);
              Serial.println(ip8);              
            } 
            // Store Device-gateway
            if (input6 != NULL){
              int ip9, ip10, ip11, ip12;
              char gwCharArray[16];
              input6.toCharArray(gwCharArray, sizeof(gwCharArray));
              sscanf(gwCharArray, "%d.%d.%d.%d", &ip9, &ip10, &ip11, &ip12);
              EEPROM.write(9, ip9);
              EEPROM.write(10, ip10);
              EEPROM.write(11, ip11);
              EEPROM.write(12, ip12);
            } 
            // Store Device-dns
            if (input7 != NULL){
              int ip13, ip14, ip15, ip16;
              char dnsCharArray[16];
              input7.toCharArray(dnsCharArray, sizeof(dnsCharArray));
              sscanf(dnsCharArray, "%d.%d.%d.%d", &ip13, &ip14, &ip15, &ip16);
              EEPROM.write(13, ip13);
              EEPROM.write(14, ip14);
              EEPROM.write(15, ip15);
              EEPROM.write(16, ip16);
            } 

            // Debug print
            Serial.print(F("Input 1: "));
            Serial.println(input1);
            Serial.print(F("Input 2: "));
            Serial.println(input2);
            Serial.print(F("Input 3: "));
            Serial.println(input3);
            Serial.print(F("Input 4: "));
            Serial.println(input4);
            Serial.print(F("Input 5: "));
            Serial.println(input5);
            Serial.print(F("Input 6: "));
            Serial.println(input6);
            Serial.print(F("Input 7: "));
            Serial.println(input7);
            Serial.print(F("netmode: "));
            Serial.println(dhcpselect);       
            Serial.println(freeMemory());
          }

          // check for relay change
          if (httpReq.startsWith(F("GET /?onRelay1 "))) {
            digitalWrite(RELAY1PIN, HIGH); // sets the relay1 on
          }  
          if (httpReq.startsWith(F("GET /?offRelay1 "))) {
            digitalWrite(RELAY1PIN, LOW); // sets the relay1 off
          }  
          if (httpReq.startsWith(F("GET /?onRelay2 "))) {
            digitalWrite(RELAY2PIN, HIGH); // sets the relay2 on
          }  
          if (httpReq.startsWith(F("GET /?offRelay2 "))) {
            digitalWrite(RELAY2PIN, LOW); // sets the relay2 off
          }  

          // check if reboot-button is pressed
          if (httpReq.startsWith(F("GET /?reboot "))) {
            restartMicrocontroller();
          }  

          // send requesed data to webpage
          if (httpReq.startsWith(F("GET /getData "))) {      
            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-Type: application/json"));
            client.println(F("Connection: close"));
            client.println();

            // Construct JSON response
            client.print(F("{\"relay1status\":\""));
            if (digitalRead(RELAY1PIN) == LOW) {
              client.print(F("OFF"));
            }
            if (digitalRead(RELAY1PIN) == HIGH) {
              client.print(F("ON"));
            }

            client.print(F("\", \"relay2status\":\""));
            if (digitalRead(RELAY2PIN) == LOW) {
              client.print(F("OFF"));
            }
            if (digitalRead(RELAY2PIN) == HIGH) {
              client.print(F("ON"));
            }

            client.print(F("\", \"mqtt_connected\":\""));
            if (!mqttClient.connected()) {
              client.print(F("No"));
            }
            if (mqttClient.connected()) {
              client.print(F("Yes"));
            }

            client.print(F("\", \"mqtt_status\":\""));
            int8_t mqttstate = mqttClient.state();
            if (mqttstate == -4) { client.print(F("MQTT_CONNECTION_TIMEOUT")); }
            if (mqttstate == -3) { client.print(F("MQTT_CONNECTION_LOST")); }
            if (mqttstate == -2) { client.print(F("MQTT_CONNECT_FAILED")); }
            if (mqttstate == -1) { client.print(F("MQTT_DISCONNECTED")); }
            if (mqttstate == 0) { client.print(F("MQTT_CONNECTED")); }
            if (mqttstate == 1) { client.print(F("MQTT_CONNECT_BAD_PROTOCOL")); }
            if (mqttstate == 2) { client.print(F("MQTT_CONNECT_BAD_CLIENT_ID")); }
            if (mqttstate == 3) { client.print(F("MQTT_CONNECT_UNAVAILABLE")); }
            if (mqttstate == 4) { client.print(F("MQTT_CONNECT_BAD_CREDENTIALS")); }
            if (mqttstate == 5) { client.print(F("MQTT_CONNECT_UNAUTHORIZED")); }

            client.print(F("\", \"mqtt_current_ip\":\""));
            client.print(retrieveFromEEPROM1(32));

            client.print(F("\", \"mqtt_current_topic\":\""));
            client.print(retrieveFromEEPROM1(96));

            client.print(F("\", \"mqtt_current_update_interval\":\""));
            byte lowByte = EEPROM.read(17);
            byte highByte = EEPROM.read(18);
            unsigned int interval = (highByte << 8) | lowByte;
            client.print(String(interval));

            client.print(F("\", \"device_current_networkmode\":\""));
            if (EEPROM.read(0) == 0) {
              client.print(F("DHCP"));
            }
            if (EEPROM.read(0) == 1) {
              client.print(F("Static"));
            }

            client.print(F("\", \"device_current_ip\":\""));
            client.print(Ethernet.localIP());

            client.print(F("\", \"device_current_subnet\":\""));
            client.print(Ethernet.subnetMask());

            client.print(F("\", \"device_current_gw\":\""));
            client.print(Ethernet.gatewayIP());

            client.print(F("\", \"device_current_dns\":\""));
            client.print(Ethernet.dnsServerIP());

            client.println(F("\"}"));      
          }


/*

          // Send webpage
          // ethernet library changes SPI frequency every time its used,
          // usualy way higher then what EEPROM is capaple of, therefore
          // it is necessary to change SPI frequency before reading from EEPROM
          // this might not be a problem with other EEPROMs, the one used for
          // prototyping is a 25LC640 (max 3Mhz)
          // other like the 25LC512 can go as high as 20Mhz, while the ethernet
          // is set for 14Mhz
          if (httpReq.startsWith("GET / ")) {
            for (int i = 0; i < 7730; i++) {
              SPI.beginTransaction(SPISettings(ExtEEPROMSPIclockfrequency, MSBFIRST, SPI_MODE0));
              digitalWrite(EEPROM_CS_PIN, LOW);
              SPI.transfer(3); // (READ);
              SPI.transfer16(i); // Address
              char data = SPI.transfer(0); // (data);
              digitalWrite(EEPROM_CS_PIN, HIGH);
              client.print(data);
            }
          }
*/
/*
          if (httpReq.startsWith("GET / ")) {        
            char buffer[16];
            int len = strlen_P(webpage);
            for (int i = 0; i < len; i += sizeof(buffer) - 1) {
              strncpy_P(buffer, webpage + i, sizeof(buffer) - 1);
              buffer[sizeof(buffer) - 1] = '\0';
              client.print(buffer);
            }
          }
*/

          if (httpReq.startsWith(F("GET / "))) {        
            const int chunkSize = 32;  // Number of bytes to read at a time, safe for ATmega328's TWI buffer
            char buffer[chunkSize + 1]; // Buffer to store read bytes (+1 for null terminator)

            for (int i = 0; i < 7864; i += chunkSize) {
              int address = i;
              int bytesToRead = min(chunkSize, 7864 - address); // Calculate how many bytes to read in this iteration

              Wire.beginTransmission(EEPROM_I2C_ADDRESS);
              Wire.write((address >> 8) & 0xFF);  // MSB of the address
              Wire.write(address & 0xFF);         // LSB of the address
              Wire.endTransmission();

              Wire.requestFrom(EEPROM_I2C_ADDRESS, bytesToRead); // Request the specified number of bytes
              for (int j = 0; j < bytesToRead; j++) {
                if (Wire.available()) {
                  buffer[j] = Wire.read(); // Read each byte into the buffer
                } else {
                  buffer[j] = 0xFF; // Default value if no data is available
                }
              }
              buffer[bytesToRead] = '\0'; // Null terminate the buffer for safe printing
              client.print(buffer); // Print the buffer to the Serial monitor
              //delay(1); // Small delay to ensure reliability
            }
          }



/*
          // give an update on RAM when page is reloaded
          Serial.print(F("Free memory: "));
          Serial.print(freeMemory());
          Serial.println(F(" bytes"));
          */

          break; // Exit the while loop
        }
      }
    }
  }
}

void storeInEEPROM(int address, const String &data) {
  int i;
  for (i = 0; i < data.length(); i++) {
    EEPROM.write(address + i, data[i]);
  }
  // Write a null character at the end to mark the end of the string
  EEPROM.write(address + i, '\0');
}

String retrieveFromEEPROM1(int address) {
  String data = "";
  char ch = EEPROM.read(address);
  while (ch != '\0' && address < EEPROM.length()) {
    data += ch; // Append the read character to the string
    address++; // Move to the next address
    ch = EEPROM.read(address); // Read the next character
  }
  return data;
}

void retrieveFromEEPROM2(int startAddress, char* buffer, int bufferSize) {
  for (int i = 0; i < bufferSize - 1; i++) {
    char ch = EEPROM.read(startAddress + i);
    buffer[i] = ch;
    if (ch == '\0') break;
  }
  buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void setup() {
  wdt_disable();  // Disable the Watchdog Timer

  Serial.begin(115200);

  delay(1000);

  pinMode(RELAY1PIN, OUTPUT);  // sets the digital pin 13 as output
  pinMode(RELAY2PIN, OUTPUT);  // sets the digital pin 13 as output

  // Start SPI at 1 MHz
//  SPI.beginTransaction(SPISettings(ExtEEPROMSPIclockfrequency, MSBFIRST, SPI_MODE0));
  SPI.begin();

  // init I2C
  Wire.begin();
  Wire.setClock(i2cclockfrequency);


/* USED WITH SPI-EEPROM
  // init External EEPROM
  pinMode(EEPROM_CS_PIN, OUTPUT);
  pinMode(EEPROM_WP_PIN, OUTPUT);
  digitalWrite(EEPROM_CS_PIN, HIGH);
  digitalWrite(EEPROM_WP_PIN, LOW);
*/

  // Configure the W5500 reset pin
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, HIGH); // Keep reset pin high

  Ethernet.init(W5500_CS);

  // Reset the W5500 Ethernet module
  digitalWrite(W5500_RST, LOW); // Set reset pin low
  delay(50);                    // Hold low for 50ms
  digitalWrite(W5500_RST, HIGH); // Set reset pin high


  // check if DHCP is selected and init network
  if (EEPROM.read(0) != 1) {
    Serial.println(F("DHCP"));
    // Start the Ethernet connection using DHCP
    if (Ethernet.begin(mac) == 0) {
      Serial.println(F("Failed to configure Ethernet using DHCP"));
      while (true);
    }
  }

  // check if Static IP is selected and init network
  if (EEPROM.read(0) == 1) {
    Serial.println(F("Static"));
    // Start the Ethernet connection using static IP
    byte ip[] = {EEPROM.read(1), EEPROM.read(2), EEPROM.read(3), EEPROM.read(4) };
    byte sub[] = {EEPROM.read(5), EEPROM.read(6), EEPROM.read(7), EEPROM.read(8) };
    byte gw[] = {EEPROM.read(9), EEPROM.read(10), EEPROM.read(11), EEPROM.read(12) };
    byte dns[] = {EEPROM.read(13), EEPROM.read(14), EEPROM.read(15), EEPROM.read(16) };
    Ethernet.begin(mac, ip, dns, gw, sub);
  }

  server.begin();
  Serial.print(F("Ethernet local IP is "));
  Serial.println(Ethernet.localIP());

    // Set the MQTT server and callback
    char mqttServer[64];
    retrieveFromEEPROM2(32, mqttServer, sizeof(mqttServer));
    mqttClient.setServer(mqttServer, 1883);
    mqttClient.setCallback(callback);

    // Connect to MQTT
    connectToMqtt();
}

void loop() {

  // check if relay is toggled
  int currentStaterelay1 = digitalRead(RELAY1PIN); // Read the current state of the input pin
  int currentStaterelay2 = digitalRead(RELAY2PIN); // Read the current state of the input pin
  char mqttTopic[64];
  retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));
  if (lastStaterelay1 == HIGH && currentStaterelay1 == LOW) {
    publishToSubtopic(mqttTopic, "/relay1", "OFF");
    Serial.println(F("relay1: 0"));
  }
  if (lastStaterelay1 == LOW && currentStaterelay1 == HIGH) {
    publishToSubtopic(mqttTopic, "/relay1", "ON");
    Serial.println(F("relay1: 1"));
  }  
  if (lastStaterelay2 == HIGH && currentStaterelay2 == LOW) {
    publishToSubtopic(mqttTopic, "/relay2", "OFF");
    Serial.println(F("relay2: 0"));
  }
  if (lastStaterelay2 == LOW && currentStaterelay2 == HIGH) {
    publishToSubtopic(mqttTopic, "/relay2", "ON");
    Serial.println(F("relay2: 1"));
  }  
  lastStaterelay1 = currentStaterelay1;
  lastStaterelay2 = currentStaterelay2;


  unsigned long currentMillismeasure = millis();
  byte lowByte = EEPROM.read(17);
  byte highByte = EEPROM.read(18);
  unsigned int interval = (highByte << 8) | lowByte;
  if (interval == 0) {
    interval = 1;
  }
  // MQTT publishing
  if (!mqttClient.connected()) {
    connectToMqtt();
  }
  mqttClient.loop();
  if (currentMillismeasure - previousMillismeasure >= (interval * 1000)) {
    previousMillismeasure = currentMillismeasure;


    // Publish data
    if (digitalRead(RELAY1PIN) == LOW){
      char mqttTopic[64];
      retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));
      publishToSubtopic(mqttTopic, "/relay1", "OFF");
    }
    if (digitalRead(RELAY1PIN) == HIGH){
      char mqttTopic[64];
      retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));      
      publishToSubtopic(mqttTopic, "/relay1", "ON");
    }
    if (digitalRead(RELAY2PIN) == LOW){
      char mqttTopic[64];
      retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));      
      publishToSubtopic(mqttTopic, "/relay2", "OFF");
    }
    if (digitalRead(RELAY2PIN) == HIGH){
      char mqttTopic[64];
      retrieveFromEEPROM2(96, mqttTopic, sizeof(mqttTopic));      
      publishToSubtopic(mqttTopic, "/relay2", "ON");
    }  


/*
    // give an update on RAM when page is reloaded
    Serial.print(F("Free memory: "));
    Serial.print(freeMemory());
    Serial.println(F(" bytes"));
*/
  }


  // Listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println(client);
    processClientRequest(client);
    delay(1);
    client.stop();
  }
}