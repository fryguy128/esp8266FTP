/**
 * This file modifies SurferTim's FTP code (http://playground.arduino.cc/Code/FTP) 
 * to work with the esp8266 chip.This only contains the functionality to upload 
 * a file to your server but additional functionality could easily be added by 
 * modifying the code starting at line 148. Functionality would be implemented using
 * raw FTP commands. A list of such commands can be found at: http://www.nsftools.com/tips/RawFTP.htm
 * 
 * Posted 28 February 2017 by Patrick Duensing(fryguy128)
 */
#include <ESP8266WiFi.h>
#include <FS.h>
#define FTPWRITE

//Change these variables to meet your needs
//#############################################
//Wifi
//I'm on a school network so I don't have a password,
//you'll have to modify the wifi connection if you have one.
const char* ssid = "SSID";

//FTP stuff
const char* host = "FTP_SERVER_URL";
const char* userName = "USERNAME";
const char* password = "PASSWORD";

//File Operation
char fileName[13] = "data.txt";

//############################################
//DO NOT CHANGE THESE
char outBuf[128];
char outCount;
WiFiClient client;
WiFiClient dclient;

//###########################################
//Move into setup()
void setup() {
// Serial setup
  Serial.begin(115200);
  delay(100);

//Wifi Setup
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //SPIFF setup
  SPIFFS.begin();
}

//#########################################
//Move into loop()
/*
 *In the current state, this loop will append
 *'Success!!' to the end of a filename then 
 *upload it to your FTP server
 */
void loop() {
  File f = SPIFFS.open(fileName, "a");
    if (!f) {
      Serial.println("file open failed");
    }
  f.println("Success!!");
  f.close();

  if(doFTP()) Serial.println(F("FTP OK"));
  else Serial.println(F("FTP FAIL"));

  delay(10000);
}

//#######################################
/*
 * This is the function where the magic happens.
 * all credit goes to SurferTim. This will open
 * and write the file to your FTP server
 */
byte doFTP()
{
    File fh = SPIFFS.open(fileName, "r");
    if (!fh) {
      Serial.println("file open failed");
    }
  if (client.connect(host,21)) {
    Serial.println(F("Command connected"));
  } 
  else {
    fh.close();
    Serial.println(F("Command connection failed"));
    return 0;
  }

  if(!eRcv()) return 0;

  client.print("USER ");
  client.println(userName);
  
  if(!eRcv()) return 0;
  
  client.print("PASS ");
  client.println(password);
  
  if(!eRcv()) return 0;
  
  client.println("SYST");

  if(!eRcv()) return 0;

  client.println("Type I");

  if(!eRcv()) return 0;

  client.println("PASV");

  if(!eRcv()) return 0;

  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL,"(,");
    array_pasv[i] = atoi(tStr);
    if(tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));    

    }
  }

  unsigned int hiPort,loPort;
  hiPort=array_pasv[4]<<8;
  loPort=array_pasv[5]&255;
  Serial.print(F("Data port: "));
  hiPort = hiPort|loPort;
  Serial.println(hiPort);
  if(dclient.connect(host, hiPort)){
    Serial.println("Data connected");
  }
  else{
    Serial.println("Data connection failed");
    client.stop();
    fh.close();
  }
  
  client.print("STOR ");
  client.println(fileName);
  if(!eRcv())
  {
    dclient.stop();
    return 0;
  }
  Serial.println(F("Writing"));
  
  byte clientBuf[64];
  int clientCount = 0;
  
  while(fh.available())
  {
    clientBuf[clientCount] = fh.read();
    clientCount++;
  
    if(clientCount > 63)
    {
      dclient.write((const uint8_t *)clientBuf, 64);
      clientCount = 0;
    }
  }
  if(clientCount > 0) dclient.write((const uint8_t *)clientBuf, clientCount);

  dclient.stop();
  Serial.println(F("Data disconnected"));
  client.println();
  if(!eRcv()) return 0;

  client.println("QUIT");

  if(!eRcv()) return 0;

  client.stop();
  Serial.println(F("Command disconnected"));

  fh.close();
  Serial.println(F("File closed"));
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;

  while(!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;      
      outBuf[outCount] = 0;
    }
  }

  if(respCode >= '4')
  {
    efail();
    return 0;  
  }

  return 1;
}


void efail()
{
  byte thisByte = 0;

  client.println(F("QUIT"));

  while(!client.available()) delay(1);

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println(F("Command disconnected"));
}
