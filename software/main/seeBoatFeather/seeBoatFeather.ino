//Code by Laura
//Started June 2017
//Attempt to get the seeBoat from feather working

//Edits from Rod Fall 2017
//Revised the turbidty part
//Add pH part
//Added the conductivity part
//Data assemble all goes together now
//watch out for the timing in the pH probe--wrong timing means we'll see zero values
//Switched over to the LED strip
//GPS worked okay as long as all the I2C stuff was plugged in okay, didn't see any weird dropping off behavior
//Also changed code to work with the LED strand

//TO DO: Verify that turbidity is working correctly--the code oddly measures one thing twice.

//Parts:
//feather radio: sents data to central feather attached to computer
//feather GPS module: time and location data
//seeBoat breakout:
//+water +temperature +conductivity +turbidity
//+LED color changing
//+turns data into something that can be sent

//////////////////////////////////////////////////////////////////////////////////////////////////
///// MUST SWTICH FROM I2C library to WIRE library!! (for temp sensor). M0 doesn't like I2C
///// GPS HATES DELAYS, WE MUST GET RID OF THEM!!
//////////////////////////////////////////////////////////////////////////////////////////////////


#include <Wire.h>
//#include <I2C.h> //This doesn't work with the M0!  I think we'll have to switch over to the Wire library
#include <RH_RF95.h>
#include <math.h>;
#include <Adafruit_GPS.h>
//#include "SoftPWM.h"
//For radio
#include <SPI.h>
//For radio sleep
//#include "RTCZero.h"
//RTCZero rtc;
#include <Adafruit_DotStar.h> //LED Strip Library
#include <RGBConverter.h>
//GPS stuff
//Adafruit_GPS GPS(&Serial1);
// what's the name of the hardware serial port?
#define GPSSerial Serial1
#define address 99 //This is the wire address for our pH sensor 
// Connect to the GPS on the hardware port
Adafruit_GPS GPS(&GPSSerial);



// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO false
// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
uint32_t timer = millis();


//SEEBOAT LED VARIABLES
int outputRed = 12;
int outputGreen = 6;
//int outputBlue = 9;
int outputBlue = 9;
int red = 0;
int green = 0;
int blue = 0;
int yellow = 0;
int hue = 0;
uint32_t starttime; 
//temperature color range (times 10; in oF)
int lowReading1dec = 660;
int highReading1dec = 870;

//SEEBOART SENSOR VARIABLES
const int waterPin = 2;     // the number of the pushbutton pin
// variables will change:
int waterVal = 0;         // variable for reading the pushbutton status


float tempVal;
float condVal;
uint8_t turbVal;
float pHVal;
double GPSlat;
double GPSlong;
int GPShour;
int GPSmin;
int GPSsec;
int GPSms;
//String GPStime; //??what's the best format for this actually?
//char GPStime[12]; //??what's the best format for this actually?

//temperature variables from past code
//const int  AT30TS750_I2C =  0x48;    // I2C Address for the temperature sensor
const int  AT30TS750_I2C =  0x4B;    // I2C Address for the temperature sensor

const byte REG_TEMP      =  0x00;    // Register Address: Temperature Value
const byte REG_CONFIG    =  0x01;    // Register Address: Configuration
byte   tempLSByte       = 0;
byte   tempMSByte       = 0; 
float  floatTemperature = 0.0000;

/*
const byte   AT30TS750_I2Cc        = 0x4B; //1001000
const byte   AT30TS750_REG_TEMP   = 0x00;  // Register Address: Temperature Value 0x00
const byte   AT30TS750_REG_CONFIG = 0x01;  // Register Address: Temperature sensor configuration
const byte   AT30TS750_REG_TEMP_RESOLUTION = 0x60; //0x00 for 9 bit; 0x60 for 12-bit
byte         timeStamp[7];                 // Byte array holding a full time stamp.
unsigned int errorCount           = 0;     // Large integer to hold a running error count
byte         errorStatus          = 0;     // Byte value to hold any returned I2C error code
byte temperature[2]; //Byte array holding the temperature; use more or less of this depending on precision.  uint_8 for whole number
//temp oC*10 for each sensor
int tempC1decA = 0;
int tempC1decB = 0;
int tempC1decC = 0;
int tempC1decD = 0;
*/





//RADIO VARIABLES
//For radio
//Message variables
//We will save up to 10 datapoints from the past so we can average values to get light color if we want
//our message will include: deviceID, time, waterCheck, temperature, turbidity, conductivity, GPSlat, GPSlong (8 items)
const int messageLength = 8;
const int messageNum = 10;
double dataArray[messageNum][messageLength];  //two dimensional array; previously uint8_t
double tempDataArray[messageLength];    //array to hold the data while we're putting it together
uint8_t tempDataArrayInt[messageLength];  //array to hold the multiplied up integer version of the data so we can send it with less pain.
//Hold a space to put the received message values until they're stored
//uint8_t tempMessageReceive[messageLength];

//////DATA STRUCTURE
//see: http://playground.arduino.cc/Code/Struct
//also: https://stackoverflow.com/questions/13775893/converting-struct-to-byte-and-back-to-struct
//from moteino code Struct_Send
/*
typedef struct {
  int           nodeId; //store this nodeId
  unsigned long uptime; //uptime in ms
  float         temp;   //temperature maybe?
} Payload;
Payload theData;
*/

//version with the seeboat data
typedef struct {
  int           deviceID; //store this nodeId
  float GPSlat; //latitude
  float GPSlong;   //longitude
  //send all the time parts individually; strings have been annoying to send
  int GPShour;
  int GPSmin;
  int GPSsec;
  int GPSms;
  float tempVal; //temperature value
  float condVal;
  float pHVal;
  float milliIrradiance;
} Payload;
Payload theData;



/*struct dataPoint {
  byte deviceID;
  byte GPSlat;
  byte GPSlong;  
};
*/


/*
 * I have to figure out what type of array this is--has int and ? cast everything to string to send? what sends best?
  tempDataArry[1] =  GPStime;
  tempDataArry[3] =  tempVal;
  tempDataArry[4] =  condVal;
  tempDataArry[5] =  turbVal;
  tempDataArry[6] = GPSlat;
  tempDataArry[7] =  GPSlong;
*/




//also a counter to keep track of where we are in the message string
int charCounter = 0;
//and a counter to keep track of which message we're on
//the most recent message will always be stored in slot 0!! everything else gets pushed back
int messageCounter = 0;
//the number of non-99 characters in any particular message
int realMessageLength = 0;
int j = 0; //a counter that's used in the playback loops

//keep track of what mode we're in; FALSE is composing, TRUE is playback
boolean playMode = false;
//values from inside certain slots in the array
int arrayVal = 0;

//Variables for the radio sending/receiving
//variable that attaches the deviceID and the message; this is what will be sent
// for feather m0
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);
// Blinky on receipt
//#define LED 13
//for Tx
int16_t packetnum = 0;  // packet counter, we increment per xmission
//variable for telling us what mode we're in
//0 is Rx; 1 is Tx
int transMode = 0;

//Variable to indicate what board number this is
//This will be different for each SeeBoat!!!!!! It lets us know who the message is coming from 
const int deviceID = 444;

//pH Variables
char ph_data[20];                //we make a 20 byte character array to hold incoming data from the pH circuit.
byte in_char = 0; 
byte i = 0; 
uint32_t pH_timer = millis();
bool ask_for_data = 1;

//Conductivity Variables
int power = 5;              //50% square wave (yellow wire)¯Â
int sensor = A1;             //final voltage (purple wire)
float val = 0.0;

//turbidity variables
float kilohertz;
float milliIrradiance;
float MainSensor;

//////////////////LED Strip variables
#define DATAPIN    11
#define CLOCKPIN   13
#define NUMPIXELS 5
Adafruit_DotStar strip = Adafruit_DotStar(
  NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

  RGBConverter converter;
  byte rgb[3];
//////////////////////////////////////////////////////////////////////////////////////////////////
// the setup routine runs once when you press reset:
void setup() {
  pinMode(10,INPUT); //pin for turbidity
  //GPS start up
  GPSsetup();

  //LED start up
  ledStartup();
  
  //radio start-up
  radioStartup();

  //Start-up the temperature sensor and the other sensors (make sure it matches the address in the function)
  sensorSetup();


  //Clear the data array
  //I need this to be a two dimensional array (matrix) now--stores the current data and x past data for averaging as needed
/*
  for (int m = 0; m < messageLength; m++) {
    tempDataArray[m] = 99;
    //tempMessageReceive[m] = 99;
  }
  for (int r = 0; r < messageNum; r++) {
    for (int k = 0; k < messageLength; k++) {
      dataArray[r][k] = 99;
    }
  }
*/
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
#endif

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP
  for(int i = 0;i<5;i++){
    strip.setPixelColor(i,0xFFFFFF);
    strip.show();
  }
  starttime = millis();
  
}

//uint_8 only goes to 255
//////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  
  //ledThreeColorTest();

//  radioListen();  //we won't need to use this unless maybe we want to try to change the temperature ranges or the variable we're looking at via radio

//get the sensor info
  measureWater();
  measurepH();
  
  measureConductivity();
    //Measure and print the temperature
   
    measureTemp(); 
//    Serial.println("Temp is here");
//     Serial.println(tempVal);
//     Serial.print(",");
  
//turn the temperature into a red-green color
//temperature input is in degrees C...maybe times 10? tempVal
  tempC1decToColor(tempVal * 10);

  measureTurbidity();
  //Get the GPS data ready
 
  GPSread();
 
  
  // approximately every 2 seconds, finalize the GPS data, put everything together, and send the data over radio
  //2000 is too much probably--the map gets pretty spread out.
  //500 is too small, the data doesn't all make it in time
 
  if (millis() - timer > 1000) {
    timer = millis(); // reset the timer
    
    GPSprintDays(); //print out day, time, antenna etc

  //If we have good location data, update it
  //(add something for if we don't have good GPS data!!)
    if (GPS.fix) {
      GPSprintLoc(); //print out location and movement info
      GPSlat = convertDegMinToDecDeg(GPS.latitude);
      Serial.println("lat:");
      Serial.println(GPSlat,5);
      GPSlong = convertDegMinToDecDeg(GPS.longitude);
      Serial.println("lon:");
      Serial.println(GPSlong,5);
  //Add time here, possibly date
    GPShour =  GPS.hour;
    GPSmin = GPS.minute;
    GPSsec = GPS.seconds;
    GPSms = GPS.milliseconds;

//      GPStime = "";
//      GPStime =  GPStime + (String)GPS.hour + (String)':' + (String)GPS.minute + (String)':'+ (String)GPS.seconds+(String)'.' + (String)GPS.milliseconds;
  //assign to variable
  //String thisTime = (String)GPS.hour + ':' + (String)GPS.minute + ':'+ (String)GPS.seconds+'.' + (String)GPS.milliseconds;
  //strcpy( GPStime, (char)thisTime);
//      GPStime =  GPS.hour + ':' + GPS.minute + ':'+ GPS.seconds+'.' + GPS.milliseconds;
//      Serial.println(GPStime);

  
    }
  
    //put all the data together
    dataAssemble();
    
    //Also do the radio send at this time
    radioSend();
  
  }



}


