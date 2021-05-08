/*
 * OK_Version 
 * MQTT => 192.168.178.35
 * Http-Post => 192.168.178.25:3000 HTTP Webserver / MariaDB
 * https://github.com/jehy/co2-online-display
 * https://github.com/jehy/arduino-esp8266-mh-z19-serial
 * 
 */

#include <Arduino.h>
#include "MHZ19.h"                                        
#include <SoftwareSerial.h>                                // Remove if using HardwareSerial
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>    
#include "ArduinoTrace.h"                                  // TRACE() and DUMP(val) Macros

  #define DATA_SERVER "192.168.xxx.yyy"        // Synology webserver@port 3000 (normally 80)
  #define WEB_SERVER_PORT 3000                //--- otherwise port 80 (hhtp)
  
  #define DATA_URL "/send.php"
  #define DATA_SENSOR_ID    100
  
  #define RX_PIN            D7               // Rx pin which the MHZ19 Tx pin is attached to
  #define TX_PIN            D8               // Tx pin which the MHZ19 Rx pin is attached to
  #define MHZ_BITRATE       9600             // Device to MH-Z19 Serial baudrate (should not be changed)
  #define intervalPublish   60000            // ms. send value every minute
  #define firmware_version "Firmware Version 1.0" 
  
  #define temp_offset 3   //--- removing offset for mounting in enclosure
  
  #define wifi_ssid         "myssid"
  #define wifi_password     "mypasswd"
  
  #define mqtt_server       "my_pi_ip_addr"
  #define mqtt_user         "pi"
  #define mqtt_password     "my_pi_passwd"
  #define mqtt_topic_main   "SmartHome/Sensors/AirQuality/"
  #define mqtt_clientname   "ESP_Sensor_01"

  char          mqtt_topic[40];   
  byte          mac[6];
  char          *_macString;  
  
  unsigned long getDataTimer    = 0;
  unsigned long currentMillis   = 0;
  unsigned long previousMillis  = 0;

  WiFiClient      esp_Client;     //--- connect to Synology
  WiFiClient      mqtt_client;    //--- connect to FHEM mqtt2_client
  PubSubClient    client(mqtt_client);
  MHZ19           myMHZ19;                                             // Constructor for library
  SoftwareSerial  mySerial(RX_PIN, TX_PIN);                   // (Uno example) create device to MH-Z19 serial

//---------------------------------------------------------------------------------------------------------------
void printWifiData()
  {
    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("  IP Address: ");
    Serial.println(ip);

    // print your MAC address:
    byte mac[6];
    WiFi.macAddress(mac);
    Serial.print("  MAC address: ");
    Serial.print(mac[5], HEX);
    Serial.print(":");
    Serial.print(mac[4], HEX);
    Serial.print(":");
    Serial.print(mac[3], HEX);
    Serial.print(":");
    Serial.print(mac[2], HEX);
    Serial.print(":");
    Serial.print(mac[1], HEX);
    Serial.print(":");
    Serial.println(mac[0], HEX);
  }
//---------------------------------------------------------------------------------------------------------------
void printCurrentNet()
{
    Serial.println("Current net:");
    // print the SSID of the network you're attached to:
    Serial.print("  SSID: ");
    Serial.println(WiFi.SSID());

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("  signal strength (RSSI):");
    Serial.println(rssi);

    // print the encryption type:
    /*byte encryption = WiFi.encryptionType();
        Serial.print("Encryption Type:");
        Serial.println(encryption,HEX);
        Serial.println();*/
}
//---------------------------------------------------------------------------------------------------------------
bool sendData(DynamicJsonDocument root)
{
  TRACE(); 
  Serial.println("Starting connection to web server...");
  if (!esp_Client.connect(DATA_SERVER, WEB_SERVER_PORT))
  {
    Serial.println("WEB: Failed to connect to server");
    return false;
  }
  else
  {
    Serial.println("WEB: succeeded to connect to web-server");
  }

  Serial.println("WEB: connected to http-server");
  //--- Make a HTTP request:
  /*
    client.println("GET /send.php?data={\"id\":1,\"temp\":" + String(t) + ",\"humidity\":" + String(h) + ",\"ppm\":" + String((int)ppm) +
                 ",\"mac\":\"" + String(macStr) + "\",\"FreeRAM\":\"" + String(mem) + "\",\"SSID\":\"" + WiFi.SSID() + "\"} HTTP/1.1");
  */
  esp_Client.println("POST " + String(DATA_URL) + " HTTP/1.1");
  esp_Client.println("Host: " + String(DATA_SERVER));
  esp_Client.println("Connection: close");
  esp_Client.println("User-Agent: Arduino/1.0");
  esp_Client.println("Content-Type: application/x-www-form-urlencoded;");

  esp_Client.print("Content-Length: ");
  String data;
  serializeJson(root, data);

  Serial.println("WEB: data to send:"); Serial.println(data);
  
  data = "data=" + data;
  esp_Client.println(data.length());
  esp_Client.println();
  esp_Client.println(data);
  
  while (esp_Client.connected() && !esp_Client.available()) //see https://github.com/esp8266/Arduino/issues/4342
  {
    delay(100);
  }
  Serial.println("WEB-Server reply:");
  Serial.println("");
  while (esp_Client.available())
  {
    char c = esp_Client.read();
    Serial.print(c);
  }
  esp_Client.stop();
  return true;
}
//---------------------------------------------------------------------------------------------------------------
void setup_wifi() {
  delay(10);
  //--- start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);                      //--- make sure the ESP isn't exposed as a access point
  WiFi.begin(wifi_ssid, wifi_password);

  if (WiFi.status() != WL_CONNECTED)
    Serial.println("WIFI not ready. Waiting.");
    
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);

  _macString = NULL;
  if (_macString != NULL)        
    delete[] _macString;        
  if (_macString == NULL)
    _macString = new char[20];
      
  sprintf(_macString, "%02x:%02x:%02x:%02x:%02x:%02x", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  
  DUMP(_macString);

  printWifiData();
  printCurrentNet();      
}
//---------------------------------------------------------------------------------------------------------------
void reconnect() 
{
  TRACE(); 
  //--- loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("mqtt: try connection...");
    //--- attempt to connect
    if (client.connect(mqtt_clientname, mqtt_user, mqtt_password)) {
      Serial.println("mqtt: connected");
    } 
    else 
    {
      Serial.print("mqtt: failed, rc=");
      Serial.print(client.state());
      Serial.println("mqtt: try again in 5 seconds");
      //--- wait 5 seconds before retrying
      delay(5000);
    }
  }
}
//---------------------------------------------------------------------------------------------------------------
void blinkLed(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}
//---------------------------------------------------------------------------------------------------------------
void mqtt_publish(int co2ppm, int8_t temp)
{
  strcpy(mqtt_topic, mqtt_topic_main);
  strcat(mqtt_topic,"co2");
  client.publish(mqtt_topic, String(co2ppm).c_str(), true);
  
  DUMP( String(mqtt_topic).c_str() );
  
  //DUMP( String(mqtt_topic).c_str() + String(co2ppm).c_str() ); 
  
  strcpy(mqtt_topic, mqtt_topic_main);
  strcat(mqtt_topic,"temperature");
  client.publish(mqtt_topic, String(temp).c_str(), true);
  
  //DUMP( String(mqtt_topic).c_str() + String(temp).c_str() ); 
  DUMP( String(mqtt_topic).c_str() ); 
}
//---------------------------------------------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);                                   // Device to serial monitor feedback
    while (!Serial) ; 
    delay(2000); 
    Serial.println(); 

    TRACE(); 
    
    setup_wifi();
    
    TRACE();     
    client.setServer(mqtt_server, 1883);
    TRACE(); 
    
    if (!client.connected()) 
    {
      DUMP("mqtt: loop-try-reconnect"); 
      reconnect();
      delay(1000); 
    }
    client.loop();

    strcpy(mqtt_topic, mqtt_topic_main);
    strcat(mqtt_topic,"info");
    client.publish(mqtt_topic, firmware_version, true);

    mySerial.begin(MHZ_BITRATE);                            // (Uno example) device to MH-Z19 serial start   
    myMHZ19.begin(mySerial);                                // *Serial(Stream) refence must be passed to library begin(). 

    myMHZ19.autoCalibration(false);                         // Turn auto calibration ON (OFF autoCalibration(false))
}
//---------------------------------------------------------------------------------------------------------------
void loop()
{
    if (millis() - getDataTimer >= 5000) 
    {
      if (!client.connected()) 
      {
        reconnect();
      }
      client.loop();
    }
  
    yield();
  
    if (millis() - getDataTimer >= 10000)
    {
        //TRACE(); 
        
        /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even 
        if below background CO2 levels or above range (useful to validate sensor). You can use the usual documented command with getCO2(false) */

        int     co2_val   = myMHZ19.getCO2();              //--- request CO2 (as ppm)
        int8_t  temp_val  = myMHZ19.getTemperature();      //--- request Temperature (as Celsius)
        temp_val -= temp_offset; 
        
        //TRACE();

        //--- to RaspberryPi and FHEM
        mqtt_publish(co2_val,temp_val); 
        
        //TRACE(); 
        
        Serial.print("CO2: "); Serial.print(co2_val);Serial.print("\t"); Serial.print("Temp: "); Serial.print(temp_val); Serial.print("\n");   

        //TRACE();

        //--- prepare for http post to synology web server
        /*char *macString;
        macString = NULL;
        if (macString != NULL)        
          delete[] macString;        
        if (macString == NULL)
          macString = new char[20];
        */
        
        //TRACE(); 
        
        DynamicJsonDocument root(200);
        root["id"]       = DATA_SENSOR_ID;
        root["temp"]     = temp_val; //String(temp_val).c_str();
        root["humidity"] = 10;
        root["ppm"]      = co2_val; //String(co2_val).c_str();
        root["mac"]      = _macString; //String(macString).c_str();
        root["FreeRAM"]  = system_get_free_heap_size();               //String(system_get_free_heap_size()).c_str();
        root["SSID"]     = WiFi.SSID();
        
        //TRACE();
        
        bool sentOk = sendData(root);        
        
        Serial.println("WEB: Request sent");
        if (sentOk)
            Serial.println("WEB: url_send ok!");
        else
            Serial.println("WEB: url_send not ok!");                                   

        getDataTimer = millis();
    }
}
//---------------------------------------------------------------------------------------------------------------
