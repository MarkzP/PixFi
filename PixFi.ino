/*************************************************************************************
 * 
 * PixFi - simple esp8266 based WiFi bridge for PX4 and derivatives
 * By Marc Paquette  marc @^ dacsystemes.com
 *
 * Compiled with Arduino 1.6.7 & esp8266 Core 2.1.0 (Stable) https://github.com/esp8266/Arduino
 *
 * This code is released under GPL v3.0: https://gnu.org/licenses/gpl-3.0.txt
 * 
 *************************************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

//Access Point SSID
#define AP_SSID "PixFi"
//(Optional) Access Point Password
//#define AP_PWD "secret_pwd"

//Serial data rate - This should match parameter SERIALx_BAUD
#define BAUD_RATE 921600  //SERIALx_BAUD = 921
//#defile BAUD_RATE 57600 //SERIALx_BAUD = 57

//Hardware flow control can be used to substantially increase throuput.
//Without this, it seems that comm speed tops up at 57600
//To use hardware flow control on the Pixhawk, connect:
// ESP GPIO 15 (RTS) -> Pixhawk Telem(x) pin 4 (CTS) (2k or less pulldown required, or else module won't boot)
// ESP GPIO 13 (CTS) <- Pixhawk Telem(x) pin 5 (RTS)
//To use hardware flow control on the Pixracer: (Untested!)
// ESP GPIO 15 (RTS) -> 8266 pin 10 (CTS)
// ESP GPIO 13 (CTS) <- 8266 pin 9 (RTS)
//Parameter BRD_SERx_RTSCTS should be set to 1
// Note: these pins cannot be changed; also, they are not broken out on the ESP-01 board (I use an ESP-03)
// Note: this requires esp8266 Arduino Core 2.1.0 (Stable) or better. It will NOT work with earlier versions.
#define ENABLE_HW_FLOW_CONTROL
//esp8266 hardware FIFO is 128 bytes
#define RTS_THRESHOLD 120

//These are the default ports used by Mission Planner & QGroundControl
#define CLIENT_PORT 14555
#define HOST_PORT 14550

//These values have been confirmed to work well
#define MAX_UDP_SIZE 1500
#define SERIAL_TIMEOUT 5

//Local variables
WiFiUDP udp;
IPAddress clientIP;
byte buf[MAX_UDP_SIZE];
int len;
unsigned long lastRx;
unsigned long lastUDP;


// ******************** Helpers ********************************

//Forwards UDP packets to the Serial port.
void UDPtoSerial()
{
  int udp_count = udp.parsePacket();
  if(udp_count)
  {
      //Save Client IP
      if(clientIP[3] == 255)
      {
          clientIP = udp.remoteIP();
      }
      
      //Send to Serial Port
      while(udp_count--)
      {
          Serial.write(udp.read());
      }

      lastUDP = millis();

      //Yield
      delay(0);
  }

  if (clientIP[3] != 255 && (millis() - lastUDP) > 5000)
  {
      //Broadcast again if we haven't got any UDP packets for more than 5 seconds
      clientIP[3] = 255;    
  }
}

//Forwards Serial data as UDP packets
void SerialToUDP()
{
  while (Serial.available() && len < MAX_UDP_SIZE)
  {
      buf[len++] = Serial.read();
      lastRx = millis();   
  }

  if (len && ((millis() - lastRx) > SERIAL_TIMEOUT || len > (MAX_UDP_SIZE - 128)))
  {
      //Yield
      delay(0);
      
      //Send UDP
      udp.beginPacket(clientIP, HOST_PORT);
      udp.write(buf, len);
      udp.endPacket();
      len = 0;
  }
}



// ******************** Arduino ********************************

void setup() {  
	delay(250);

  //Setup Serial port
	Serial.begin(BAUD_RATE);

  #ifdef ENABLE_HW_FLOW_CONTROL
    //CTS Flow Control
    pinMode(13, FUNCTION_4);
    USC0(0) |= (1 << UCTXHFE);
    
    //RTS Flow Control
    pinMode(15, FUNCTION_4);
  
    #ifdef RTS_THRESHOLD
      USC1(0) |= ((RTS_THRESHOLD & 0x7F) << UCRXHFT);
    #endif
    
    USC1(0) |= (1 << UCRXHFE);  
  #endif

  //Setup AP
  #ifdef AP_PWD
  	WiFi.softAP(AP_SSID, AP_PWD);
  #else
    WiFi.softAP(AP_SSID);
  #endif

  //Set Client IP as broadcast address of softAP subnet (x.x.x.255)
	clientIP = WiFi.softAPIP();
  clientIP[3] = 255;

  //Listen for UDP on port CLIENT_PORT
  udp.begin(CLIENT_PORT);

  len = 0;
}

//Loop!
void loop()
{
  UDPtoSerial();
  SerialToUDP();
}
