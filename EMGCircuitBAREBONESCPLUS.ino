
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

LSM6DS3 myIMU(SPI_MODE,33); //Default constructor is I2C, addr 0x6B

// Set these to your desired credentials.
const char *ssid = "GGesp32";
const char *password = "capstone";

WiFiServer server(80);
WebServer server2(81);





//Init Running Average
//RunningAverage A3_RA(3);
RunningMedian A3_RM(5);
int samples = 0;

// Initialize Analog Pins
int EMGLeftRA_pin = 39;
int EMGRightRA_pin = 34;
int EMGLeftOb_pin = 36;
int EMGRightOb_pin = 13;
int EMGErect_pin = 32;

//adc1_channel_t adcEMG1_pin = ADC1_CHANNEL_3; 

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

/*
 * Login page
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

double ReadVoltage(int reading){
  //double reading = analogRead(pin); // Reference voltage is 3v3 so maximum reading is 3v3 = 4095 in range 0 to 4095
  if(reading < 1 || reading > 4095) return 0;
  // return -0.000000000009824 * pow(reading,3) + 0.000000016557283 * pow(reading,2) + 0.000854596860691 * reading + 0.065440348345433;
  return -0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089;
}

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
    // Setup Serial which is useful for debugging
    // Use the Serial Monitor to view printed messages
    Serial.begin(115200);
    //while (!Serial) ; // wait for serial port to connect. Needed for native USB
    Serial.println("start");
    Serial.println("Configuring access point...");
//
//  // Check if the init and set mode are actually needed later
//  wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
//  esp_wifi_init(&wifiInitConfig);
//  esp_wifi_set_mode (WIFI_MODE_APSTA);
//  esp_wifi_set_ps (WIFI_PS_NONE);

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server2.on("/", HTTP_GET, []() {
    server2.sendHeader("Connection", "close");
    server2.send(200, "text/html", loginIndex);
  });
  server2.on("/serverIndex", HTTP_GET, []() {
    server2.sendHeader("Connection", "close");
    server2.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server2.on("/update", HTTP_POST, []() {
    server2.sendHeader("Connection", "close");
    server2.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server2.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }

  });

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

 //Start Servers
  server.begin();
  server2.begin();
  Serial.println("Server started");
  Serial.println("Web Server started");


}

// Main logic of your circuit. It defines the interaction between the components you selected. After setup, it runs over and over again, in an eternal loop.
void loop() 
{
  
  
  WiFiClient client = server.available();   // listen for incoming clients
  server2.handleClient();
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

    //Method 1
    EMGleftRA=analogRead(EMGLeftRA_pin);
    EMGrightRA=analogRead(EMGRightRA_pin);
    EMGleftob=analogRead(EMGLeftOb_pin);
    EMGrightob=analogRead(EMGRightOb_pin);
    EMGerect=analogRead(EMGErect_pin);
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
    
    // Converts 5 bytes of data in str to a char array e
    str1.toCharArray(a,5);
    str2.toCharArray(b,5);
    str3.toCharArray(c,5);
    str4.toCharArray(d,5);
    str5.toCharArray(e,5);
    
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
    Serial.println(e);
    

    //ENDING DATA TRANSMISSION 
    client.write("\r");
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
