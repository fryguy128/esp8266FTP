/**
 * This sketch reads input from a MAX1820 temperature sensor then saves it to a file
 * which is then uploaded to an FTP server. Once a week, the data file will be renamed
 * and uploaded. This creates a new file for every week's worth of data.
 * 
 * Other's sketches that made this possible include:
 *   SurferTim's FTP
 *   Michael Margolis, Tom Igoe, and Ivan Grokhotkov's FTP script
 *   
 * 
 * Posted 09 May 2017 by Patrick Duensing (fryguy128)
  */
#include <ESP8266WiFi.h>
#include <FS.h>
#include <Time.h>
#include <WiFiUdp.h>
#include <OneWire.h>

//Change these variables to meet your needs
//#############################################
//Wifi
//I'm on a school network so I don't have a password,
//you'll have to modify the wifi connection if you have one.
const char* ssid = "YOUR SSID";

//FTP stuff
const char* host = "YOUR FTP HOST";
const char* userName = "YOUR FTP USERNAME";
const char* password = "YOUR FTP PASSWORD";
const int timeZone = 0;     // UTC

//############################################
//DO NOT CHANGE THESE UNLESS YOU KNOW WHAT YOU'RE DOING
//FTP buffers
char outBuf[128];
char outCount;
//WiFi Clients
WiFiClient client;
WiFiClient dclient;
//NTP Stuff
unsigned int localPort = 2390; //UDP Port
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP udp; // A UDP instance to let us send and receive packets over UDP
//LED
int led = 2;
//Temperature
OneWire  ds(14);


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

  //UDP setup
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  //led Pin setup
  pinMode(led, OUTPUT);

}

//#########################################
//Move into loop()
void loop() {
  //This is the filename where your newest data will be stored
  String fileNameData = "/data.txt";
  //This is the filename that contains a single number used to create a new file
  String textVar = "/number.txt";
  //This string will hold the name of the new file to be created at the end of a week
  String newFileName = "";

  //Turn the led on for the entire loop
  digitalWrite(led, HIGH);
  
  //Get the time from the NTP server
  setSyncProvider(getNtpTime);
  Serial.println("1) NTP GOTTEN");

  //This runs if the time was found. If it wasn't, the date would be set to sometime in 1970.
  if (year() != 1970) {
    Serial.println("2) YEAR NOT 1970");

    //Used to determine if a week has passed
    int count = 0;

    //Opens the data file to read
    File j = SPIFFS.open(fileNameData, "r");
    if (!j) {
      Serial.println("file open failed");
    }
    else {
      //Finds out how many records there are in the main data file
      while (j.available()) {
        count++;
        j.readStringUntil('\n');
      }
      j.close();
    }
    
      Serial.println(count);
      if (count > 1007) {                                        // 1008 records is equal to a week of recording every 10 minutes
        File h = SPIFFS.open(textVar, "r");                      // Open the file holding the number to read
        if (!h) {
          Serial.println("file open failed");
        }
        else {
            int inputVal = h.parseInt();                         // Read the integer from the number file
            //Close the number file
            h.close();
            Serial.println(inputVal);                            // Create the new file name
            newFileName = "/data" + String(inputVal);
            newFileName += ".txt";
            Serial.println("newFileName = " + newFileName);
            SPIFFS.rename(fileNameData, newFileName);           // Rename the data file
            File x = SPIFFS.open(textVar, "w");                 // Reopen the number file in write mode
            if(!x){
              Serial.println("file open failed");
            }
            else{
              inputVal++;                                      // Increment the value read in from the number file
              x.println(inputVal);                             // Write the number to the number file
              x.close();                                       // Closes the number file
            }
        }
      }
    File f = SPIFFS.open(fileNameData, "a");                   // Open the data file in append mode
    if (!f) {
      Serial.println("file open failed");
    }
    else {
      Serial.println("3) FILE OPENED");
      getTemperature(f);                                       //Call the getTemperature function with the data file as input
      
      //This section puts the date and time into the data file
      if (hour() < 10) {                                       //For uniformity put a 0 in front of the hour before printing it to the file if it is before 10am (24hr time is used)
        f.print("0");
        f.print(hour());
      }
      else {
        f.print(hour());
      }
      printDigits(minute(), f);
      printDigits(second(), f);
      f.print(" ");
      if (month() < 10) {                                     //For uniformity if it is not yet october, put a 0 in front of the month
        f.print("0");
        f.print(month());
      }
      else {
        f.print(month());
      }
      f.print("/");
      if (day() < 10) {                                       //For uniformity if it is not the 10th of the month yet, put a 0 in front of the day
        f.print("0");
        f.print(day());
      }
      else {
        f.print(day());
      }
      f.print("/");
      f.print(year());
      f.println();
      f.close();                                             //Close the data file, writing is done
      Serial.println("5) FILE PRINTED AND CLOSED");

      if (doFTP(fileNameData)) Serial.println(F("FTP Data OK"));            //Upload the data file using the doFTP function
      else Serial.println(F("FTP Data FAIL"));
      
      if (newFileName.length() > 0) {                                       //If it has been a week, upload the new data file using the doFTP function
        if (doFTP(newFileName))  Serial.println(F("FTP DataNum OK"));
        else Serial.println(F("FTP DataNum FAIL"));
      }
      Serial.println("6) FTP DONE");
    }
  }
  //This executes if the NTP time was not received
  else {
    Serial.println("2) TIME FAILED");
    for (int i = 0; i < 10; i++) {                        //Blink the led 10 times
      digitalWrite(led, LOW);
      delay(500);
      digitalWrite(led, HIGH);
      delay(500);
    }
  }
  digitalWrite(led, LOW);                                 //Turn off the LED

  Serial.println("3/7) GOING TO SLEEP");
  ESP.deepSleep(600000000);                               //Puts the ESP8266 into sleep mode for 10 minutes
}

//#######################################
/*
   This is the function where the magic happens.
   all credit goes to SurferTim. This will open
   and write the file to your FTP server
*/
byte doFTP(String fileName)
{
  File fh = SPIFFS.open(fileName, "r");
  if (!fh) {
    Serial.println("file open failed");
    return 0;
  }
  if (client.connect(host, 21)) {
    Serial.println(F("Command connected"));
  }
  else {
    fh.close();
    Serial.println(F("Command connection failed"));
    return 0;
  }

  if (!eRcv()) return 0;

  client.print("USER ");
  client.println(userName);

  if (!eRcv()) return 0;

  client.print("PASS ");
  client.println(password);

  if (!eRcv()) return 0;

  client.println("SYST");

  if (!eRcv()) return 0;

  client.println("Type I");

  if (!eRcv()) return 0;

  client.println("PASV");

  if (!eRcv()) return 0;

  char *tStr = strtok(outBuf, "(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL, "(,");
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));
      return 0;
    }
  }

  unsigned int hiPort, loPort;
  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;
  Serial.print(F("Data port: "));
  hiPort = hiPort | loPort;
  Serial.println(hiPort);
  if (dclient.connect(host, hiPort)) {
    Serial.println("Data connected");
  }
  else {
    Serial.println("Data connection failed");
    client.stop();
    fh.close();
    return 0;
  }

  client.print("STOR ");
  client.println(fileName);
  if (!eRcv())
  {
    dclient.stop();
    return 0;
  }
  Serial.println(F("Writing"));

  byte clientBuf[64];
  int clientCount = 0;

  while (fh.available())
  {
    clientBuf[clientCount] = fh.read();
    clientCount++;

    if (clientCount > 63)
    {
      dclient.write((const uint8_t *)clientBuf, 64);
      clientCount = 0;
    }
  }
  if (clientCount > 0) dclient.write((const uint8_t *)clientBuf, clientCount);

  dclient.stop();
  Serial.println(F("Data disconnected"));
  client.println();
  if (!eRcv()) return 0;

  client.println("QUIT");

  if (!eRcv()) return 0;

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

  while (!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while (client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);

    if (outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  if (respCode >= '4')
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

  while (!client.available()) delay(1);

  while (client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println(F("Command disconnected"));
}

//#################################################################

void printDigits(int digits, File f) {
  // utility for digital clock display: prints preceding colon and leading 0
  f.print(":");
  if (digits < 10)
    f.print('0');
  f.print(digits);
}
/*-------- NTP code ----------*/


time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}


/**
 * This function reads the temperature from the MAX1820
 * sensor then prints it to a specified file.
 * 
 * Receives: A File object
 * Returns: Nothing
 */
void getTemperature(File fh) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  //Gets addresses of OneWire sensors
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }

  //Prints the address of the OneWire sensors to the serial output
  Serial.print("ROM =");
  for ( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  //Checks the validity an address
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  
  //Calculate celsius and fahrenheit values then print them to a file
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  fh.print(celsius);
  fh.print("\t");
  fh.print(fahrenheit);
  fh.print("\t");
}
