//using simcom SIM808 module for GSM and GPS
//using Atmega328p microcontroller. Tested with Arduino nano.
#include "gsmMqtt.h" //mqtt client library
#include <SoftwareSerial.h> 
//#include<stdio.h>
//#include<string.h>
#define DEBUG true
#define sosButton 7
#define gpsPin 5
SoftwareSerial mySerial(2, 3); //Sim808 UART connected to software serial pins 2,3. Tx to Rx and Rx to Tx.
/*------------- USER INPUT --------------------------------*/
char* deviceName = "flipGps1";//use a unique client name for your device
char* mqttBroker = "52.xxx.xxx.160"; // mqtt broker ip or URL
char* mqttPort = "1883"; //mqtt broker port
char* mqttTopic = "flip/gsm/gps";//topic to which you want to publish
String sosNum = "ATD9xxxxxxxx9;";
/*---------------------------------------------------------*/
char type[32], no1[32], no2[32], date[32], lat[32], lon[32];

String mySerialStr = "";
//char Message[300];
int index = 0;
byte data1;
char atCommand[50];
byte mqttMessage[127];
int mqttMessageLength = 0;


//bolean flags
boolean gprsReady = false;
boolean mqttSent = false;

void setup()
{
  pinMode(sosButton, INPUT);
  pinMode(gpsPin, OUTPUT);
  digitalWrite(sosButton, HIGH);
  digitalWrite(gpsPin, LOW);
  delay(5000);
  digitalWrite(gpsPin, HIGH);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  Serial.begin(9600);
  mySerial.begin(9600);
  Serial.print("GPS/GSM/GPRS Initializing");
  for (int i = 0; i < 10; i++) //wait for 10 seconds so GSM is completely initialized
  {
    Serial.print(".");
    delay(10);
  }
  Serial.println("");
  call();//do a simple call to get AGPS fix.
  delay(20000);
  hangup();
  Serial.println("");
  Serial.println("Ready!");
}

void loop()
{
  sos();//make a call if button pressed.
  getgps();//Get GPS data
  String x = sendData( "AT+CGNSINF", 1000, DEBUG);
  //+CGNSINF: 1,1,20161122182451.000,13.01xxxx,77.6xxxxx,919.200,0.15,10.5,1,,0.9,2.0,1.8,,12,9,,,47,,
  char* buf = x.c_str();
  strcpy(type, strtok(buf , ":"));
  strcpy(no1, strtok(NULL, ","));
  strcpy(no2 , strtok(NULL, ","));
  strcpy(date , strtok(NULL, ","));
  strcpy(lat, strtok(NULL, ","));
  strcpy(lon, strtok(NULL, ","));

  Serial.print("Date/Time: "); Serial.println(date);
  Serial.print("Latitude : "); Serial.println(lat);
  Serial.print("Longitude: "); Serial.println(lon);
  Serial.println("");
  sendGprsData();
}


void getgps(void)
{
  sendData( "AT+CGNSPWR=1", 1000, DEBUG);//power ON GPS/GNS module
  String x = sendData( "AT+CGNSSTATUS?", 1000, DEBUG);//Check Status
  //wait till 3D or 2D fix is obtained.
  if (x != "Location 3D Fix")
  {
    String x = sendData( "AT+CGPSSTATUS?", 1000, DEBUG);
    Serial.println(x);
  }
  sendData( "AT+CGNSSEQ=RMC", 1000, DEBUG);//read GPRMC data
}


String sendData(String command, const int timeout, boolean debug)
{
  String response = "";
  mySerial.println(command);
  long int time = millis();
  while ( (time + timeout) > millis())
  {

    while (mySerial.available())
    {
      char c = mySerial.read();

      response += c;
    }
  }
  if (debug)
  {
    Serial.print(response);
  }
  return response;
}

void sendGprsData()
{
  digitalWrite(13, HIGH);
  Serial.println("Checking if GPRS is READy?");
  mySerial.println("AT");
  delay(1000);
  gprsReady = isGPRSReady();
  if (gprsReady == false)
  {
    Serial.println("GPRS Not Ready");
  }
  if (gprsReady == true)
  {
    Serial.println("GPRS READY!");
    String json = buildJson();
    char jsonStr[300];
    json.toCharArray(jsonStr, 300);
    Serial.println(json);
    //*arguments in function are:
    //clientID, IP, Port, Topic, Message
    sendMQTTMessage(deviceName, mqttBroker, mqttPort, mqttTopic, jsonStr);
  }
  //delay (1000);// delay between two mqtt publish
  digitalWrite(13, LOW);
}
boolean isGPRSReady()
{
  mySerial.println("AT");
  mySerial.println("AT");
  mySerial.println("AT+CGATT?");
  index = 0;
  while (mySerial.available())
  {
    data1 = (char)mySerial.read();
    Serial.write(data1);
    mySerialStr[index++] = data1;
  }
  Serial.println("Check OK");
  Serial.print("gprs str = ");
  Serial.println(data1);
  if (data1 > -1)
  {
    Serial.println("GPRS OK");
    return true;
  }
  else
  {
    Serial.println("GPRS NOT OK");
    return false;
  }
}

void sendMQTTMessage(char* clientId, char* brokerUrl, char* brokerPort, char* topic, char* message)
{
  mySerial.println("AT"); // Sends AT command to wake up cell phone
  Serial.println("send AT to wake up mySerial");
  delay(1000); // Wait a second
  // digitalWrite(13, HIGH);
  mySerial.println("AT+CSTT=\"www\",\"\",\"\""); // Puts phone into mySerial mode
  Serial.println("AT+CSTT=\"www\",\"\",\"\"");
  delay(2000); // Wait a second
  mySerial.println("AT+CIICR");
  Serial.println("AT+CIICR");
  delay(2000);
  mySerial.println("AT+CIFSR");
  Serial.println("AT+CIFSR");
  delay(1000);
  strcpy(atCommand, "AT+CIPSTART=\"TCP\",\"");
  strcat(atCommand, brokerUrl);
  strcat(atCommand, "\",\"");
  strcat(atCommand, brokerPort);
  strcat(atCommand, "\"");
  mySerial.println(atCommand);
  Serial.println(atCommand);
  // Serial.println("AT+CIPSTART=\"TCP\",\"mqttdashboard.com\",\"1883\"");
  delay(1000);
  mySerial.println("AT+CIPSEND");
  Serial.println("AT+CIPSEND");
  delay(1000);
  mqttMessageLength = 16 + strlen(clientId);
  Serial.println(mqttMessageLength);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++)
  {
    mySerial.write(mqttMessage[j]); // Message contents
    //Serial.write(mqttMessage[j]); // Message contents
    //Serial.println("");
  }
  mySerial.write(byte(26)); // (signals end of message)
  Serial.println("Sending message");
  delay(1000);
  mySerial.println("AT+CIPSEND");
  Serial.println("AT+CIPSEND");
  delay(1000);
  mqttMessageLength = 4 + strlen(topic) + strlen(message);
  Serial.println(mqttMessageLength);
  mqtt_publish_message(mqttMessage, topic, message);
  for (int k = 0; k < mqttMessageLength; k++)
  {
    mySerial.write(mqttMessage[k]);
    Serial.write((byte)mqttMessage[k]);
  }
  mySerial.write(byte(26)); // (signals end of message)
  Serial.println("");
  Serial.println("-------------Sent-------------"); // Message contents
  delay(2000);
  //mySerial.println("AT+CIPCLOSE");
  //Serial.println("AT+CIPCLOSE");
  delay(1000);
}

String buildJson() {
  String data = "{";
  data += "\n";
  data += "\"ID\":\"FlipGNS1\",";
  data += "\n";
  data += "\"Date\":\"";
  data += date;
  data += "\",\n";
  data += "\"Latitude\":\"";
  data += lat;
  data += "\",\n";
  data += "\"Longitude\":\"";
  data += lon;
  data += "\"\n";
  data += "}";
  return data;
}

void sos()
{
  int xx = digitalRead(sosButton);
  if (xx == 0)
  {
    Serial.println("SOS Activated");
    call();
    delay(60000);
    hangup();
    Serial.println("SOS Ended");
  }
  xx == 1;
}


void call(void)
{
  sendData( "AT+CSQ", 1000, DEBUG);//check signal quality
  Serial.println("Making a call!");
  mySerial.println("ATD9663984729;"); // xxxxxxxxx is the number you want to dial.
  if (mySerial.available())
    Serial.print((unsigned char)mySerial.read());
}

void hangup(void)
{
  mySerial.println("ATH"); //End the call.
  if (mySerial.available())
    Serial.print((unsigned char)mySerial.read());
}

