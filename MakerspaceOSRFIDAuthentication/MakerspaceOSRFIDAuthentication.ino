
/* 
*
* Copyright (C) 2015 MakerSpace OS
*
* This software may be modified and distributed under the terms
* of the MIT license.  See the LICENSE file for details.
*/

/*
*  bradley.hess@gmail.com
*/

 /* Pin layout should be as follows:
 * Signal     Pin              Pin               Pin
 *            Arduino Uno      Arduino Mega      MFRC522 board
 * ------------------------------------------------------------
 * Reset      6                5                 RST  Changed to 6 from 9
 * SPI SS     7                53                SDA  Changed to 7 from 10
 * SPI MOSI   11               51                MOSI
 * SPI MISO   12               50                MISO
 * SPI SCK    13               52                SCK
 *
 * The reader can be found on eBay for around 5 dollars. Search for "mf-rc522" on ebay.com. 
 */

#include <SPI.h>
#include <Ethernet.h> 
#include <MFRC522.h> //https://github.com/miguelbalboa/rfid rc522 Library
#include <SoftwareSerial.h>  //http://arduino.cc/en/Reference/softwareSerial If you have Software Serial LCD screen attached.
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <Keypad.h>  //http://playground.arduino.cc/Code/Keypad  If you have a Keypad attached.
//#include <CountDownTimer.h> // http://playground.arduino.cc/Main/CountDownTimer Used for controlling how long a used has access to the equipment
#include "AccessResponse.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetClient client;

//The IP address of the authentication server running the MakerspaceOS Authentication Service
IPAddress server(10,0,0,10);

//The IP address to assign if DHCP does not work.
IPAddress ip(10,0,0,3);


//RFID reader pins
#define RFID_SS_pin 7
#define RFID_RST_pin 6

//Ethernet sheild pins
#define ETH_SS_pin 10

//Relay Pin
#define RELAY1  A5


//Initialize the RFID reader
MFRC522 mfrc522(RFID_SS_pin, RFID_RST_pin);  

//Settings-- Adjust as needed.  
const String EQUIPMENTID = "1212";
#define USEWEBAUTHENTICATE


#define USELED
//#define USEPINPAD  //Comment out if a pinpad is not being used
#define USELCD   // Comment out if not using LCD
#define DEBUG   //Comment this line out to remove debug serial and LCD print outs
#define CHECKCURRENT
#define USETONE
#ifdef USETONE
  #define TONEPIN 8
#endif

#ifdef USELED
  #define RED_LED A0 
  #define GREEN_LED A2
  #define YELLOW_LED A1
#endif

#ifdef CHECKCURRENT
  //Current detection pin.  Used if you want to detect if the equipment is in used before turning it off.  Important for equipment where turning it off midway through usage could be dangerous.  AKA tablesaw :)
  #define currentPin  A4
#endif

#ifdef USEPINPAD
  //Keypad setup if used, otherwise comment out.
  const byte rows = 4; //four rows
  const byte cols = 3; //three columns
  char keys[rows][cols] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
  };
  byte rowPins[rows] = {5, A0, A3, 0}; //connect to the row pinouts of the keypad
  byte colPins[cols] = {3, 4, 9}; //connect to the column pinouts of the keypad
   //connect to the column pinouts of the keypad
  Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, rows, cols );
#endif

unsigned long Watch, _micro, time = micros();
unsigned int Clock = 0, R_clock;
boolean Reset = false, Stop = false, Paused = false;
volatile boolean timeFlag = false;
String storedSerial;

      
#ifdef USELCD
  // Attach the serial display's RX line to digital pin 2
  SoftwareSerial lcdSerial(3,2); 
#endif



void setup()
{
  
  pinMode(RELAY1, OUTPUT); 
  digitalWrite(RELAY1,LOW);
  
  #ifdef DEBUG  //Only setup serial if in debug
    Serial.begin(9600);
  #endif
  
  //Only setup LCD if used
  #ifdef USELCD
    lcdSerial.begin(9600); // set up serial port for 9600 baud
    delay(500); // wait for display to boot up
    ClearScreen();
    lcdSerial.write("Establishing Connection");
  #endif
  
  // Disable RFID board
  pinMode(RFID_SS_pin, OUTPUT);
  digitalWrite(RFID_SS_pin, HIGH);

  //Enable and intialize Ethernet.  
  pinMode(ETH_SS_pin, OUTPUT);
  digitalWrite(ETH_SS_pin, LOW);
  // Ethernet init
  if (!Ethernet.begin(mac)) {
      Ethernet.begin(mac, ip);
      
      #ifdef USELCD  //Only if we are using an LCD
          ClearScreen();
          lcdSerial.write("Connection Failed");
      #endif 
      #ifdef DEDBUG  // Only in Debug mode
          Serial.println("Failed to configure Ethernet using DHCP");
      #endif 
  }    
  else
  {  
       String DHCP  = "";
        for (byte thisByte = 0; thisByte < 4; thisByte++) {
             // print the value of each byte of the IP address:
             DHCP = DHCP + String(Ethernet.localIP()[thisByte], DEC);
             DHCP = DHCP + "."; 
         }
       char IP[DHCP.length()+1];
       DHCP.toCharArray(IP, DHCP.length()+1);
      #ifdef USELCD //Only if we are using an LCD
         ClearScreen();
         lcdSerial.write(IP);
         delay(2000);
      #endif
      #ifdef DEBUG
        Serial.println(IP);
      #endif
  }
  
   // give the Ethernet shield a second to initialize:
   delay(1000);
   #ifdef DEBUG
     Serial.println("connecting...");
   #endif
   
   if (client.connect(server, 80)) {
     #ifdef DEBUG
       Serial.println("connected to server");
     #endif
     #ifdef USELCD
       ClearScreen();
       lcdSerial.write("connected to server");
     #endif
   } 
   else {
     #ifdef DEBUG
       Serial.println("connection failed");
     #endif
     #ifdef USELCD
       ClearScreen();
       lcdSerial.write("connection failed");
     #endif
   }

  // Disable ethernet
  digitalWrite(ETH_SS_pin, HIGH);
  // Enable and then init RFID
  digitalWrite(RFID_SS_pin, LOW);
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  #ifdef USELED
    pinMode(RED_LED, OUTPUT); 
    pinMode(GREEN_LED, OUTPUT); 
    pinMode(YELLOW_LED, OUTPUT);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, HIGH); 
    delay(2000);
  #endif
  #ifdef USELCD
    ClearScreen();
    lcdSerial.write("Please scan card");
  #endif
  Stop = true;
}


void loop()
{
  CountDownTimer(); // run the timer
  if (TimeHasChanged()) 
  { 
    #ifdef DEBUG
      Serial.print(ShowHours());
      Serial.print(":");
      Serial.print(ShowMinutes());
      Serial.print(":");
      Serial.print(ShowSeconds());
      Serial.print(":");
      Serial.print(ShowMilliSeconds());
      Serial.print(":");
      Serial.println(ShowMicroSeconds());
    #endif
    
    // This DOES NOT format the time to 0:0x when seconds is less than 10.
    // if you need to format the time to standard format, use the sprintf() function.
    if(Stop == true)
    {
      #ifdef DEBUG
        Serial.println("Stop reached");
      #endif
      #ifdef USELED
     
      #endif 
       digitalWrite(RELAY1,LOW);
    }
  }
   
      int gotCard = 0;
      String serial;
      String pin ="";
      
      // Disable ethernet
      digitalWrite(ETH_SS_pin, HIGH);
      // Enable RFID
      digitalWrite(RFID_SS_pin, LOW);

      // See if RFID card present
      if (mfrc522.PICC_IsNewCardPresent()) 
      {
        if (mfrc522.PICC_ReadCardSerial()) 
        {
          
          #ifdef USETONE
            tone(TONEPIN,200,500);
          #endif
          
          gotCard = 1;
          for (byte i = 0; i < mfrc522.uid.size; i++) 
          {
                serial += String(mfrc522.uid.uidByte[i], HEX);
                #ifdef DEBUG
                  Serial.println(mfrc522.uid.uidByte[i], HEX);
                #endif  
         
         
               #ifdef USEPINPAD
                 #ifdef USELCD
                   ClearScreen();
                   lcdSerial.write("Enter Pin + #");
                 #endif
                 char key;
                  while(key!='#')
                  {
                    key = keypad.getKey();
                    if (key){
                      if(key != '#')
                      {
                      pin = pin + key;
                      #ifdef DEBUG
                        Serial.println(key);
                      #endif
                      }
                    }
                  }
                #else
                  pin = "~";  
                #endif
                #ifdef DEBUG
                  Serial.println("Card Read...");
                #endif  
          }
      
                // Stop and disable RFID
          mfrc522.PICC_HaltA();
          #ifdef DEBUG
                 Serial.println("Read Serial:" + serial);
                 Serial.println("Stored Serial:" + storedSerial);
          #endif
               
          if(serial == storedSerial)
          {
              digitalWrite(RELAY1,LOW);
              StopTimer();
              SetTimer(0);
              storedSerial = "";
              
          }
          else
          {
          AccessResponse accessResponse;
          
          #ifdef USEWEBAUTHENTICATE
            accessResponse = CheckAccessUsingService( serial, pin);
          #endif
          
          #ifdef DEBUG
            Serial.println(accessResponse.AccessAllowed);
          #endif         
          if(strcmp(accessResponse.AccessAllowed,"true")==0)
          {
               storedSerial = serial;
               
               #ifdef DEBUG
                 Serial.println("Stored" + storedSerial);
               #endif
               
               #ifdef USELED
                 TurnOnLED(GREEN_LED);
               #endif
               #ifdef USETONE
                  tone(TONEPIN,300,1000);
               #endif
               #ifdef USELCD
                 ClearScreen();
                 lcdSerial.write("Access Granted");
                 lcdSerial.write(0x0A);
                 lcdSerial.write("Welcome ");
                 lcdSerial.write(accessResponse.Username);
                 delay(1000);
                 ClearScreen();
               #endif
               
               
               digitalWrite(RELAY1,HIGH);
              
               if(accessResponse.TimeLimit != 0)
               {
                 
                 #ifdef DEBUG
                   Serial.println(accessResponse.TimeLimit);
                 #endif
                 //Need to add a countdown timer here so that time counts down the time remaining and outputs to the LCD
                 //delay(accessResponse.TimeLimit * 60000);
                 CountDownTimer(); // run the timer
                 SetTimer(0,0,accessResponse.TimeLimit * 60); // 10 seconds
                 StartTimer();
               }
               
           }
           else{
               #ifdef USELED
                  TurnOnLED(RED_LED);
               #endif
               #ifdef USETONE
                  tone(TONEPIN,100,1000);
               #endif    
               #ifdef USELCD
                  ClearScreen();
                  lcdSerial.write("Access Denied");
                  lcdSerial.write(accessResponse.ResponseMessage);
               #endif
           }
           delay(2000);
          }
        }
      }
      #ifdef USELED
          TurnOnLED(YELLOW_LED);
      #endif
      #ifdef USELCD         
           ClearScreen();
           lcdSerial.write("Please scan card");
      #endif
}

void TurnOnLED(int LED)
{
  TurnOffAllLED();
  digitalWrite(LED,HIGH);
  
}
void TurnOffAllLED()
{
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
}

void ClearScreen()
{
  #ifdef USELCD
    lcdSerial.write(0xFE); 
    lcdSerial.write(0x01);
  #endif
}


AccessResponse CheckAccessUsingService(String serial, String pin)
{
  #ifdef USEWEBAUTHENTICATE
	// Enable ethernet
	digitalWrite(ETH_SS_pin, LOW);
	// Disable RFID
        digitalWrite(RFID_SS_pin, HIGH);

	#ifdef DEBUG
		Serial.println("/MakerSpaceOS.Services/EquipmentAccessControl.svc/CheckEquipmentAccess/" + EQUIPMENTID + "/" + serial + "/" + pin);
	#endif
	client.connect(server, 80);
	client.println("GET /MakerSpaceOS.Services/EquipmentAccessControl.svc/CheckEquipmentAccess/" + EQUIPMENTID + "/" + serial + "/" + pin);
	client.println("Host: localhost");
	client.println("Connection: close");
	client.println();

	String response = "";

        while (client.connected()) 
        {
	  // read this packet
	  while (client.available())
          {
	    char c = client.read();
	    response = response + c;
            //Serial.println(c);
          }
	}
//	#ifdef DEBUG
//		Serial.println(response);
//	#endif

	StaticJsonBuffer<200> jsonBuffer;
	char charBuf[response.length() + 1];
        #ifdef DEBUG
		Serial.println(response);
	#endif
	response.toCharArray(charBuf, response.length() + 1);
	JsonObject& root = jsonBuffer.parseObject(charBuf);

	//Enable RFID
	digitalWrite(RFID_SS_pin, HIGH);
	//Disable Ethernet
        digitalWrite(ETH_SS_pin, LOW);

	client.stop();
	
        if (!root.success()) 
        {
  	  #ifdef DEBUG
	  	Serial.println("parseObject() failed");
	  #endif

	  AccessResponse accessResponse = AccessResponse();
	  accessResponse.AccessAllowed = "false";
	  accessResponse.ResponseMessage = "Server Error Cannot Authenticate";
          return accessResponse;
	}
	AccessResponse accessResponse = AccessResponse();
	accessResponse.AccessAllowed = root["AccessAllowed"];
	accessResponse.Username = root["UserName"];
	accessResponse.ResponseMessage = root["Message"];
	accessResponse.TimeLimit = root["TimeLimit"];
	return accessResponse;
  #endif
}



boolean CountDownTimer()
{
  static unsigned long duration = 1000000; // 1 second
  timeFlag = false;

  if (!Stop && !Paused) // if not Stopped or Paused, run timer
  {
    // check the time difference and see if 1 second has elapsed
    if ((_micro = micros()) - time > duration ) 
    {
      Clock--;
      timeFlag = true;

      if (Clock == 0) // check to see if the clock is 0
        Stop = true; // If so, stop the timer

     // check to see if micros() has rolled over, if not,
     // then increment "time" by duration
      _micro < time ? time = _micro : time += duration; 
    }
  }
  return !Stop; // return the state of the timer
}
void StartTimer()
{
  Watch = micros(); // get the initial microseconds at the start of the timer
  Stop = false;
  Paused = false;
}

void StopTimer()
{
  Stop = true;
}

void SetTimer(unsigned int hours, unsigned int minutes, unsigned int seconds)
{
  // This handles invalid time overflow ie 1(H), 0(M), 120(S) -> 1, 2, 0
  unsigned int _S = (seconds / 60), _M = (minutes / 60);
  if(_S) minutes += _S;
  if(_M) hours += _M;

  Clock = (hours * 3600) + (minutes * 60) + (seconds % 60);
  R_clock = Clock;
  Stop = false;
}

void SetTimer(unsigned int seconds)
{
 // StartTimer(seconds / 3600, (seconds / 3600) / 60, seconds % 60);
 Clock = seconds;
 R_clock = Clock;
 Stop = false;
}
int ShowHours()
{
  return Clock / 3600;
}

int ShowMinutes()
{
  return (Clock / 60) % 60;
}

int ShowSeconds()
{
  return Clock % 60;
}

unsigned long ShowMilliSeconds()
{
  return (_micro - Watch)/ 1000.0;
}

unsigned long ShowMicroSeconds()
{
  return _micro - Watch;
}

boolean TimeHasChanged()
{
  return timeFlag;
}

// output true if timer equals requested time
boolean TimeCheck(unsigned int hours, unsigned int minutes, unsigned int seconds) 
{
  return (hours == ShowHours() && minutes == ShowMinutes() && seconds == ShowSeconds());
}


