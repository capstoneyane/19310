// Fill-in information from your Blynk Template here 
#define BLYNK_TEMPLATE_ID "TMPL2LIs7vzJ2"
#define BLYNK_TEMPLATE_NAME "Ya rab"

#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

#include <MQUnifiedsensor.h>
#include "BlynkEdgent.h"
#include <BlynkTimer.h>
#include <ESP8266WebServer.h>

// Define the board correctly for MQUnifiedsensor
#define placa "ESP8266"
#define Voltage_Resolution 5  // NodeMCU operates at 3.3V
#define pin A0 // Analog input 0 of your NodeMCU
#define type "MQ-135" // MQ135
#define ADC_Bit_Resolution 10 // For NodeMCU, ADC is 10-bit
#define RatioMQ135CleanAir 1.2 // RS/R0 ratio in clean air

// Initialize a separate server on port 8080
ESP8266WebServer customServer(8080);

// Declare Sensor
MQUnifiedsensor MQ135(placa, Voltage_Resolution, ADC_Bit_Resolution, pin, type);

// Sensor data variables
int eyeLevelInt = 0;
int flagInt = 0;
const short frame_check = 10;  // Corrected with type and semicolon
short prev = 0;
float CO2; // Global variable
int PulseSensorPurplePin = 0;        // Pulse Sensor PURPLE WIRE connected to ANALOG PIN 0
int LED13 = 2;   //  The on-board Arduion LED
int Signal;                // holds the incoming raw data. Signal value can range from 0-1024
int Threshold = 550;            // Determine which Signal to "count as a beat", and which to ingore.


// Declare BlynkTimer
BlynkTimer timer;

// Function to handle the root URL "/" on custom server
void handleRoot() {
  Serial.println("Handling root request on custom server");
  customServer.send(200, "text/plain", "ESP8266 is up and running on custom server!");
}

// Function to handle incoming data at "/data" on custom server
void handleData() {
  Serial.println("Received POST request at /data on custom server");
  
  if (!customServer.hasArg("plain")) {  // Check if body is present
    Serial.println("No body received in POST request");
    customServer.send(400, "text/plain", "Body not received");
    return;
  }

  String body = customServer.arg("plain");
  Serial.print("Body received: ");
  Serial.println(body);
  
  // Assuming data is sent in the format "eye_level,flag"
  // Example: "0.25,10"

  int commaIndex = body.indexOf(',');
  if (commaIndex == -1) {
    Serial.println("Invalid data format received");
    customServer.send(400, "text/plain", "Invalid data format");
    return;
  }
  
  // Extract the substrings for eyeLevel and flag
  String eyeLevelStr = body.substring(0, commaIndex);
  String flagStr = body.substring(commaIndex + 1);

  Serial.print("Parsed eyeLevelStr: ");
  Serial.println(eyeLevelStr);
  Serial.print("Parsed flagStr: ");
  Serial.println(flagStr);

  // Convert eyeLevel from String to float, then scale and convert to int
  float eyeLevelFloat = eyeLevelStr.toFloat();
  eyeLevelInt = static_cast<int>(eyeLevelFloat * 100);  // 0.25 becomes 25

  // Convert flag from String to int
  flagInt = flagStr.toInt();

  // Debugging: Print the received and converted values
  Serial.print("Received Eye Level (Float): ");
  Serial.println(eyeLevelFloat, 2);  // Print with 2 decimal places
  Serial.print("Converted Eye Level (Int): ");
  Serial.println(eyeLevelInt);
  
  Serial.print("Received Flag: ");
  Serial.println(flagInt);

  customServer.send(200, "text/plain", "Data received");
}

// Function to send sensor data to Blynk virtual pins
void sendAllSensorData() {
  Serial.println("Sending all sensor data to Blynk");
  
  // Send Eye Level
  Blynk.virtualWrite(V1, eyeLevelInt);
  
  // Send CO2 and Flag
  Blynk.virtualWrite(V0, CO2);
  Blynk.virtualWrite(V2, flagInt);

  // Send heart beats
  Blynk.virtualWrite(V3, Signal);

  // Log events based on flag, CO2 levels, and heart beats
  if (flagInt % 10 == 0 && flagInt != 0 ) {
    Blynk.logEvent("flags", "Please take a rest!");
  }
  if (CO2 > 750) {
    Blynk.logEvent("co2", "High concentration of harmful gases, please find fresh air");
  }

  if (Signal >= 110) {
    Blynk.logEvent("highhbr", "Your heartbeat rate is getting higher, please take a rest");
  }
  if (Signal <= 82 & Signal > 81) {
    Blynk.logEvent("firstsleep", "Your heartbeat rate is getting lower, please take a rest if needed");
  }
  if (Signal <= 81) {
    Blynk.logEvent("totalsleep", "Attention, your heartbeat rate is getting lower, you are possibly about to fall asleep, please take a rest");
  }
  
  

  Serial.println("All data sent to Blynk");
}

void setup() {

  pinMode(LED13,OUTPUT);         // pin that will blink to your heartbeat!
  Serial.begin(115200); // Ensure serial monitor is set to 115200 baud
  delay(500);

  // Set math model to calculate the PPM concentration and the value of constants
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  
  /*****************************  MQ Init ********************************************/ 
  // Remarks: Configure the pin of NodeMCU as input.
  /************************************************************************************/ 
  MQ135.init(); 
  /* 
    // If the RL value is different from 10K please assign your RL value with the following method:
    MQ135.setRL(10);
  */
  
  /*****************************  MQ Calibration ********************************************/ 
  // Explanation: 
  // In this routine, the sensor will measure the resistance of the sensor supposedly before being pre-heated
  // and in clean air (Calibration conditions), setting up R0 value.
  // We recommend executing this routine only on setup in laboratory conditions.
  // This routine does not need to be executed on each restart; you can load your R0 value from EEPROM.
  // Acknowledgements: https://jayconsystems.com/blog/understanding-a-gas-sensor
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i <= 10; i++) {
    MQ135.update(); // Update data; the NodeMCU will read the voltage from the analog pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
    delay(200); // Small delay between readings
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println("  done!.");
  
  if(isinf(calcR0)) {
    Serial.println("Warning: Connection issue, R0 is infinite (Open circuit detected). Please check your wiring and supply.");
    while(1);
  }
  if(calcR0 == 0){
    Serial.println("Warning: Connection issue found, R0 is zero (Analog pin shorts to ground). Please check your wiring and supply.");
    while(1);
  }
  
  /*****************************  MQ Calibration ********************************************/ 
  Serial.println("** Values from MQ-135 ****");
  Serial.println("|  CO2  |");  

  Serial.println();
  Serial.println("Initializing BlynkEdgent and Custom Server...");

  // Define custom server routes using customServer instance
  customServer.on("/", handleRoot);
  customServer.on("/data", HTTP_POST, handleData);

  // Start the custom server on port 8080
  customServer.begin();
  Serial.println("Custom HTTP server started on port 8080");

  // Initialize BlynkEdgent
  BlynkEdgent.begin();

  // Setup timer to send all sensor data every second (1000 ms)
  timer.setInterval(1000L, sendAllSensorData);
}

void loop() {
  MQ135.update(); // Update data; the NodeMCU will read the voltage from the analog pin
  
  // Configure the equation to calculate CO2 concentration value
  MQ135.setA(400); 
  MQ135.setB(0.715); 
  CO2 = MQ135.readSensor(); // Assign to the global variable

  Signal = analogRead(PulseSensorPurplePin);  // Read the PulseSensor's value.
                                              // Assign this value to the "Signal" variable.

   Serial.println(Signal);                    // Send the Signal value to Serial Plotter.


   if(Signal > Threshold){                    
     digitalWrite(LED13,LOW);
   } else {
     digitalWrite(LED13,HIGH);      
   }

  // Run BlynkTimer
  timer.run();
  
  // Run BlynkEdgent tasks
  BlynkEdgent.run();
  
  // Handle custom server clients
  customServer.handleClient();
}
