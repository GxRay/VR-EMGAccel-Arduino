
// Include Libraries
#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Update.h>
#include "SparkFunLSM6DS3.h"
#include "Wire.h"
#include "SPI.h"
#include "esp32-hal-adc.h"
#include "RunningMedian.h"
#include <driver/adc.h>
#include "esp_wifi.h"
#include <MadgwickAHRS.h>
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks

LSM6DS3 myIMU(SPI_MODE,21); //Default constructor is I2C, addr 0x6B
Madgwick filter;
float EMA_a = 0.3;    //initialization of EMA alpha
int EMA_S = 0;        //initialization of EMA S

#define EAP_ANONYMOUS_IDENTITY "anonymous@example.com"
#define EAP_IDENTITY ""
#define EAP_PASSWORD ""
const char* ssid = "RU-Secure"; // Eduroam SSID
//const char* host = "arduino.php5.sk"; //external server domain for HTTP connection after authentification
int counter = 0;
WiFiClient client;

unsigned long microsPerReading, microsPrevious;

//// Set these to your desired credentials.
//const char *ssid = "GGesp32";
//const char *password = "capstone";

WiFiServer server(80);
WebServer server2(81);


//Init Running Average
//RunningAverage A3_RA(3);
RunningMedian A3_RM(5);
int samples = 0;

// Initialize Analog Pins
int EMGLeftRA_pin = 39; //A3
int EMGRightRA_pin = 34; //A2
int EMGLeftOb_pin = 36; //A4
int EMGRightOb_pin = 13; //13
int EMGErect_pin = 32; //32

// Initialize EMG Aquisition Variables
int EMGleftRA=0;
int EMGrightRA=0;
int EMGleftob=0;
int EMGrightob=0;
int EMGerect=0;

// Initialize Mapping for EMG data
int Min = 0;
int Max = 4095;
int Map_Min= 0;
int Map_Max= 2200;

//Initialize Accelerometer Data
float AccelX=0;
float AccelY=0;
float AccelZ=0;

//Initialize Gyrometer Data
float GyroX=0;
float GyroY=0;
float GyroZ=0;

//Initialize Average Gyrometer Data
float AvgGyroX=0;
float AvgGyroY=0;
float AvgGyroZ=0;

//Initialize Average Gyrometer Data
float TAvgGyroX=0;
float TAvgGyroY=0;
float TAvgGyroZ=0;

/*
 * setup function
 */

double modifiedMap(double x, double in_min, double in_max, double out_min, double out_max)
{
 return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Setup the essentials for your circuit to work. It runs first every time your circuit is powered with electricity.
void setup() 
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.print("Connecting to network: ");
  Serial.println(ssid);
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA); //init wifi mode
 esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ANONYMOUS_IDENTITY, strlen(EAP_ANONYMOUS_IDENTITY)); 
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config settings to default
  esp_wifi_sta_wpa2_ent_enable(&config); //set config settings to enable function
  WiFi.begin(ssid); //connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if(counter>=60){ //after 30 seconds timeout - reset board
      ESP.restart();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address set: "); 
  Serial.println(WiFi.localIP()); //print LAN IP

  myIMU.settings.gyroRange = 125;   //Max deg/s.  Can be: 125, 245, 500, 1000, 2000
  myIMU.settings.gyroSampleRate = 13;   //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666
  myIMU.settings.gyroBandWidth = 50;  //Hz.  Can be: 50, 100, 200, 400;

  myIMU.settings.accelRange = 2;      //Max G force readable.  Can be: 2, 4, 8, 16
  myIMU.settings.accelSampleRate = 13;  //Hz.  Can be: 13, 26, 52, 104, 208, 416, 833, 1666, 3332, 6664, 13330
  myIMU.settings.accelBandWidth = 50;  //Hz.  Can be: 50, 100, 200, 400;


  myIMU.begin();
  filter.begin(13);

// initialize variables to pace updates to correct rate
  microsPerReading = 1000000 / 13;
  microsPrevious = micros();

//Configure RunningAverage
//A3_RA.clear();

 
 //Configuring ADC Pin
  //pinMode(EMG1_pin,INPUT);
  adcAttachPin(EMGLeftRA_pin);
  adcAttachPin(EMGRightRA_pin);
  adcAttachPin(EMGLeftOb_pin);
  adcAttachPin(EMGRightOb_pin);
  adcAttachPin(EMGErect_pin);
  analogReadResolution(12);
  analogSetAttenuation(ADC_6db);

// //Start Servers
  server.begin();
//  server2.begin();
//  Serial.println("Server started");
//  Serial.println("Web Server started");


}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop() 
{
  float roll, pitch, heading;
  unsigned long microsNow;

  if (WiFi.status() == WL_CONNECTED) { //if we are connected to Eduroam network
    counter = 0; //reset counter
    Serial.println("Wifi is still connected with IP: "); 
    Serial.println(WiFi.localIP());   //inform user about his IP address
  }else if (WiFi.status() != WL_CONNECTED) { //if we lost connection, retry
    WiFi.begin(ssid);      
  }
  while (WiFi.status() != WL_CONNECTED) { //during lost connection, print dots
    delay(500);
    Serial.print(".");
    counter++;
    if(counter>=60){ //30 seconds timeout - reset board
    ESP.restart();
    }
  }
  client = server.available();
  Serial.println (client);
  delay(1);
  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected

      
// SENDING RAW EMG DATA TO LABVIEW
    
    // Initialize buffer for the ASCII String
    char a[6]="";
    char b[6]="";
    char c[6]="";
    char d[6]="";
    char e[6]="";

    // Initializing Buffer for ASCII String for Accel Data
    char Ax[6]="";  
    char Ay[6]="";  
    char Az[6]="";

    // Initializing Buffer for ASCII String for Accel Data
    char Gx[6]="";  
    char Gy[6]="";  
    char Gz[6]="";  


  
    //Method 1 EMG
    EMGleftRA=analogRead(EMGLeftRA_pin);
    EMGrightRA=analogRead(EMGRightRA_pin);
    EMGleftob=analogRead(EMGLeftOb_pin);
    EMGrightob=analogRead(EMGRightOb_pin);
    EMGerect=analogRead(EMGErect_pin);

//// check if it's time to read data and update the filter
  microsNow = micros();
  if (microsNow - microsPrevious >= microsPerReading) {


    // Defining Positional Data
    AccelX=myIMU.readFloatAccelX(); 
    AccelY=myIMU.readFloatAccelY(); 
    AccelZ=myIMU.readFloatAccelZ();

    // Defining Positional Data
    
    GyroX=myIMU.readFloatGyroX();
    GyroY=myIMU.readFloatGyroY(); 
    GyroZ=myIMU.readFloatGyroZ();   

    filter.updateIMU(GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ);

    roll = filter.getRoll();
    pitch = filter.getPitch();
    heading = filter.getYaw();
    
    EMA_S = (EMA_a*heading)+((1-EMA_a)*EMA_S);
    heading = heading - EMA_S;

    AccelX=heading;
    AccelY=pitch;
    AccelZ=roll;

//    Serial.print("Orientation: ");
//    Serial.print(heading);
//    Serial.print(" ");
//    Serial.print(pitch);
//    Serial.print(" ");
//    Serial.println(roll);

// increment previous time, so we keep proper pace
    microsPrevious = microsPrevious + microsPerReading;
  }

    //Filter EMG
    //Serial.println(EMG1);
    //adcStart(EMG1_pin);
    //EMG1=adcEnd(EMG1_pin);
    //while (adcBusy(EMG1_pin));
    //A3_RA.addValue(adcEnd(EMG1_pin));
    //A3_RM.add(adcEnd(EMG1_pin));
    //EMG1=A3_RM.getMedian();

    //Method 2 (turning on adc1 causes 36 and 39 to pull down or something? See Section 3.11 of ECO_and_Workarounds_for_Bugs_in_ESP32)
    //EMG1=adc1_get_raw(adcEMG1_pin);
    
    // Maping vaues from 0-4012 to 0-2200Mv
    EMGleftRA=modifiedMap(EMGleftRA,Min,Max,Map_Min,Map_Max);
    EMGrightRA=modifiedMap(EMGrightRA,Min,Max,Map_Min,Map_Max);
    EMGleftob=modifiedMap(EMGleftob,Min,Max,Map_Min,Map_Max);
    EMGrightob=modifiedMap(EMGrightob,Min,Max,Map_Min,Map_Max);
    EMGerect=modifiedMap(EMGerect,Min,Max,Map_Min,Map_Max);
    
    // Converting RAW emg data to a string
    String str1= String(EMGleftRA);
    String str2= String(EMGrightRA);
    String str3= String(EMGleftob);
    String str4= String(EMGrightob);
    String str5= String(EMGerect);

    // Converting Accel Data to Strings
    String Axstr= String(AccelX);
    String Aystr= String(AccelY);
    String Azstr= String(AccelZ);

    // Converting Accel Data to Strings
    String Gyxstr= String(GyroX);
    String Gyystr= String(GyroY);
    String Gyzstr= String(GyroZ);
    
    
    // Converts 5 bytes of data in str to a char array e
    str1.toCharArray(a,5);
    str2.toCharArray(b,5);
    str3.toCharArray(c,5);
    str4.toCharArray(d,5);
    str5.toCharArray(e,5);

    // Converting Strings to Char Array
     Axstr.toCharArray(Ax,6);
     Aystr.toCharArray(Ay,6);
     Azstr.toCharArray(Az,6);

     // Converting Strings to Char Array
     Gyxstr.toCharArray(Gx,6);
     Gyystr.toCharArray(Gy,6);
     Gyzstr.toCharArray(Gz,6);
//
//    //Writing all to server EMG first then Accel then Gyro
    client.write(a);
    Serial.print(a);
    client.write(",");
    Serial.print(",");
    client.write(b);
    Serial.print(b);
    client.write(",");
    Serial.print(",");
    client.write(c);
    Serial.print(c);
    client.write(",");
    Serial.print(",");
    client.write(d);
    Serial.print(d);
    client.write(",");
    Serial.print(",");
    client.write(e);
    Serial.print(e);
    client.write(",");
    Serial.print(",");
    client.write(Ax);
    Serial.print(Ax);
    client.write(",");
    Serial.print(",");
    client.write(Ay);
    Serial.print(Ay);
    client.write(",");
    Serial.print(",");
    client.write(Az);
    Serial.print(Az);
    client.write(",");
    Serial.print(",");
    client.write(Gx);
    Serial.print(Gx);
    client.write(",");
    Serial.print(",");
    client.write(Gy);
    Serial.print(Gy);
    client.write(",");
    Serial.print(",");
    client.write(Gz);
    Serial.println(Gz);
    

    //ENDING DATA TRANSMISSION 
    client.write("\r");
    }
    // close the connection:
    //client.stop();
    //Serial.println("Client Disconnected.");
  }
}
