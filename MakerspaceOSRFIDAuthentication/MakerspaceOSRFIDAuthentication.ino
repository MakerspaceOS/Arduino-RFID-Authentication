
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
  #define RED_LED 5 
  #define GREEN_LED 4
  #define YELLOW_LED A0
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
  byte rowPins[rows] = {A1, A0, A3, 0}; //connect to the row pinouts of the keypad
  byte colPins[cols] = {3, A2, 9}; //connect to the column pinouts of the keypad
   //connect to the column pinouts of the keypad
  Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, rows, cols );
#endif


#ifdef USELCD
  // Attach the serial display's RX line to digital pin 2
  SoftwareSerial lcdSerial(3,2); 
#endif



void setup()
{
  
  pinMode(RELAY1, OUTPUT); 
  
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
      #ifdef USELCD //Only if we are using an LCD
        ClearScreen();
        String DHCP  = "";
        for (byte thisByte = 0; thisByte < 4; thisByte++) {
             // print the value of each byte of the IP address:
             DHCP = DHCP + String(Ethernet.localIP()[thisByte], DEC);
             DHCP = DHCP + "."; 
         }
       char IP[DHCP.length()+1];
       DHCP.toCharArray(IP, DHCP.length()+1);
       lcdSerial.write(IP);
       delay(2000);
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
  #endif
  #ifdef USELCD
    ClearScreen();
    lcdSerial.write("Please scan card");
  #endif
}


void loop()
{
      int gotCard = 0;
      String serial;
      String pin;
      // Disable ethernet
      digitalWrite(ETH_SS_pin, HIGH);
      // Enable RFID
      digitalWrite(RFID_SS_pin, LOW);

      // See if RFID card present
      if (mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
          
          #ifdef USETONE
            tone(TONEPIN,200,500);
          #endif
          
          gotCard = 1;
          for (byte i = 0; i < mfrc522.uid.size; i++) {
                serial += String(mfrc522.uid.uidByte[i], HEX);
                #ifdef DEBUG
                  Serial.println(mfrc522.uid.uidByte[i], HEX);
                #endif  
        } 
         #ifdef USELCD
           ClearScreen();
           lcdSerial.write("Enter Pin + #");
         #endif
         #ifdef USEPINPAD
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
          #endif
          #ifdef DEBUG
            Serial.println("Card Read...");
          #endif  
      }
      }
     
      // Stop and disable RFID
      mfrc522.PICC_HaltA();
      //Disable RFID
      digitalWrite(RFID_SS_pin, HIGH);
      // Enable Ethernet
      digitalWrite(ETH_SS_pin, LOW);
      
      if (gotCard) {
         #ifdef USELCD
           ClearScreen();
           lcdSerial.write("Starting Card Validation..");
         #endif
         
         client.connect(server, 80);
         client.println("GET /MakerSpaceOS.Services/Access.svc/CheckAccess/" + serial + "/" + pin);
         client.println("Host: localhost");
         client.println("Connection: close");
         client.println();
         
         String response = "";
        
         while(client.connected()) {
         // read this packet
          while(client.available()){
            char c = client.read();
            response= response + c;
           }
        }
         #ifdef DEBUG
          Serial.println(response);
         #endif
         
         StaticJsonBuffer<200> jsonBuffer;
         char charBuf[response.length()+1];
         
         response.toCharArray(charBuf, response.length()+1);
            #ifdef DEBUG
              Serial.println(charBuf);
            #endif
          JsonObject& root = jsonBuffer.parseObject(charBuf);

          if (!root.success()) {
             #ifdef DEBUG
                Serial.println("parseObject() failed");
             #endif
             #ifdef USETONE
                  tone(TONEPIN,100,1000);
               #endif    
             #ifdef USELCD
               ClearScreen();
               lcdSerial.write("Server Error Cannot Authenticate");
               delay(2000);
               ClearScreen();
               lcdSerial.write("Please scan card");
             #endif
            return;
          }
         
         const char* accessAllowed = root["AccessAllowed"] ;
         const char* username  =root["UserName"];
         const char* message  =root["Message"];
         #ifdef DEBUG
           Serial.println(accessAllowed );
           Serial.println( username );
         #endif
         
         client.stop();
         #ifdef USELED
           digitalWrite(YELLOW_LED, LOW);
         #endif
         
         String str(accessAllowed);
         if(str == "true"){
               #ifdef USELED
                 digitalWrite(GREEN_LED, HIGH);
               #endif
               #ifdef USETONE
                  tone(TONEPIN,300,1000);
               #endif
               #ifdef USELCD
                 ClearScreen();
                 lcdSerial.write("Access Granted");
                 lcdSerial.write(0x0A);
                 lcdSerial.write("Welcome ");
                 lcdSerial.write(username);
                 delay(1000);
                 ClearScreen();
               #endif
               digitalWrite(RELAY1,HIGH); 
               
           }
           else{
               #ifdef USELED
                  digitalWrite(RED_LED, HIGH);
               #endif
               #ifdef USETONE
                  tone(TONEPIN,100,1000);
               #endif    
               #ifdef USELCD
                  ClearScreen();
                  lcdSerial.write("Access Denied");
               #endif
           }
        delay(2000);
        #ifdef USELED
          digitalWrite(RED_LED, LOW);
          digitalWrite(GREEN_LED, LOW);
          digitalWrite(YELLOW_LED, HIGH);
        #endif
        #ifdef USELCD         
           ClearScreen();
           lcdSerial.write("Please scan card");
        #endif
      }
}

void ClearScreen()
{
  #ifdef USELCD
    lcdSerial.write(0xFE); 
    lcdSerial.write(0x01);
  #endif
}

