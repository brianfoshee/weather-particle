/******************************************************************************
  SparkFun_Photon_Weather_Basic_Soil.ino
  SparkFun Photon Weather Shield basic example with soil moisture and temp
  Joel Bartlett @ SparkFun Electronics
  Original Creation Date: May 18, 2015
  This sketch prints the temperature, humidity, barrometric preassure, altitude,
  to the Seril port.

  Hardware Connections:
  This sketch was written specifically for the Photon Weather Shield,
  which connects the HTU21D and MPL3115A2 to the I2C bus by default.
  If you have an HTU21D and/or an MPL3115A2 breakout,	use the following
  hardware setup:
  HTU21D ------------- Photon
  (-) ------------------- GND
  (+) ------------------- 3.3V (VCC)
  CL ------------------- D1/SCL
  DA ------------------- D0/SDA

  MPL3115A2 ------------- Photon
  GND ------------------- GND
  VCC ------------------- 3.3V (VCC)
  SCL ------------------ D1/SCL
  SDA ------------------ D0/SDA

  Development environment specifics:
IDE: Particle Dev
Hardware Platform: Particle Photon
Particle Core

This code is beerware; if you see me (or any other SparkFun
employee) at the local, and you've found our code helpful,
please buy us a round!
Distributed as-is; no warranty is given.
 *******************************************************************************/
#include "HTU21D.h"
#include "SparkFun_MPL3115A2.h"
#include "math.h"

//Wunderground Vars

//char SERVER[] = "rtupdate.wunderground.com";        //Rapidfire update server - for multiple sends per minute
char SERVER [] = "weatherstation.wunderground.com";   //Standard server - for sends once per minute or less
char WEBPAGE [] = "GET /weatherstation/updateweatherstation.php?";

//Station Identification
char ID [] = ""; //Your station ID here
char PASSWORD [] = ""; //your Weather Underground password here

TCPClient client;

float humidity = 0;
float tempc = 0;
float tempf = 0;
float pascals = 0;
float inches = 0;
float altf = 0;
float baroTemp = 0;
float dewptC = 0;
float dewptF = 0;

unsigned long last = 0;

HTU21D htu = HTU21D();//create instance of HTU21D Temp and humidity sensor
MPL3115A2 baro = MPL3115A2();//create instance of MPL3115A2 barrometric sensor

//---------------------------------------------------------------
void setup()
{
  Serial.begin(9600);   // open serial over USB at 9600 baud

  //Initialize both on-board sensors
  while(!htu.begin()){
    Serial.println("HTU21D not found");
    delay(1000);
  }
  Serial.println("HTU21D OK");

  while(!baro.begin()) {
    Serial.println("MPL3115A2 not found");
    delay(1000);
  }
  Serial.println("MPL3115A2 OK");

  //MPL3115A2 Settings
  baro.setModeBarometer();//Set to Barometer Mode
  //baro.setModeAltimeter();//Set to altimeter Mode

  baro.setOversampleRate(7); // Set Oversample to the recommended 128
  baro.enableEventFlags(); //Necessary register calls to enble temp, baro ansd alt
}
//---------------------------------------------------------------
void loop()
{
  //Get readings from all sensors
  //calcWeather();
  getWeather();
  //Rather than use a delay, keeping track of a counter allows the photon to
  //still take readings and do work in between printing out data.
  unsigned long now = millis();
  //alter this number to change the amount of time between each reading
  if(now - last > 10000)//prints roughly every 10 seconds
  {
    last = now;
    printInfo();
    sendToWU();
  }
}

void sendInfo()
{
  /* sprintf() doesn't work yet with %f formatter.
  char data[255];
  sprintf(data, "{\"temperature\": %d,\"humidity\": %f,\"pressure\": %f,\"altitude\": %f}", (tempf+baroTemp)/2, humidity, pascals, altf);
  */
  String d;
  d.reserve(255);
  d.concat("{");
  d.concat("\"temperature\":");
  d.concat(tempf);
  d.concat(",");
  d.concat("\"humidity\":");
  d.concat(humidity);
  d.concat(",");
  d.concat("\"pressure\":");
  d.concat(inches);
  d.concat(",");
  d.concat("\"altitude\":");
  d.concat(altf);
  d.concat("}");
  Serial.println(d);
  Particle.publish("weather.test.01", d, 60, PRIVATE);
}

void sendToWU()
{
  Serial.println("connecting...");

  if (client.connect(SERVER, 80)) {
  Serial.println("Connected");
  client.print(WEBPAGE);
  client.print("ID=");
  client.print(ID);
  client.print("&PASSWORD=");
  client.print(PASSWORD);
  client.print("&dateutc=now");      //can use 'now' instead of time if sending in real time
  client.print("&tempf=");
  client.print(tempf);
  client.print("&dewptf=");
  client.print(dewptF);
  client.print("&humidity=");
  client.print(humidity);
  client.print("&baromin=");
  client.print(inches);
  client.print("&action=updateraw");    //Standard update rate - for sending once a minute or less
  //client.print("&softwaretype=Particle-Photon&action=updateraw&realtime=1&rtfreq=30");  //Rapid Fire update rate - for sending multiple times per minute, specify frequency in seconds
  client.println();
  Serial.println("Upload complete");
  delay(300);                         //Without the delay it goes to sleep too fast and the send is unreliable
  }else{
    Serial.println(F("Connection failed"));
  return;
  }
}

//---------------------------------------------------------------
void printInfo()
{
  //This function prints the weather data out to the default Serial Port

  //Take the temp reading from each sensor and average them.
  Serial.print("Avg Temp:");
  Serial.print((tempf+baroTemp)/2);
  Serial.print("F, ");

  //Or you can print each temp separately
  Serial.print("HTU21D Temp: ");
  Serial.print(tempf);
  Serial.print("F, ");
  Serial.print("Baro Temp: ");
  Serial.print(baroTemp);
  Serial.print("F, ");

  Serial.print("Humidity:");
  Serial.print(humidity);
  Serial.print("%, ");

  Serial.print("Pressure:");
  Serial.print(pascals);
  Serial.print("Pa, ");

  Serial.print("Altitude:");
  Serial.print(altf);
  Serial.println("ft.");

}
void getWeather()
{
  // Measure Relative Humidity from the HTU21D or Si7021
  humidity = htu.readHumidity();

  // Measure Temperature from the HTU21D or Si7021
  float humTempC = htu.readTemperature();
  float humTempF = (humTempC * 9)/5 + 32;
  // Temperature is measured every time RH is requested.
  // It is faster, therefore, to read it from previous RH
  // measurement with getTemp() instead with readTemp()

  //Measure the Barometer temperature in F from the MPL3115A2
  float baroTempC = baro.readTemp();
  float baroTempF = (baroTempC * 9)/5 + 32; //convert the temperature to F

  //Measure Pressure from the MPL3115A2
  pascals = baro.readPressure();
  inches = pascals * 0.0002953; // Calc for converting Pa to inHg (Wunderground expects inHg)

  //If in altitude mode, you can get a reading in feet with this line:
  // altf = baro.readAltitudeFt();

  //Average the temperature reading from both sensors
  tempc=((humTempC+baroTempC)/2);
  tempf=((humTempF+baroTempF)/2);

  //Calculate Dew Point
  dewptC = dewPoint(tempc, humidity);
  dewptF = (dewptC * 9.0)/ 5.0 + 32.0;
}

//---------------------------------------------------------------
// dewPoint function from NOAA
// reference (1) : http://wahiduddin.net/calc/density_algorithms.htm
// reference (2) : http://www.colorado.edu/geography/weather_station/Geog_site/about.htm
//---------------------------------------------------------------
double dewPoint(double celsius, double humidity)
{
	// (1) Saturation Vapor Pressure = ESGG(T)
	double RATIO = 373.15 / (273.15 + celsius);
	double RHS = -7.90298 * (RATIO - 1);
	RHS += 5.02808 * log10(RATIO);
	RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
	RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
	RHS += log10(1013.246);

  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
	double VP = pow(10, RHS - 3) * humidity;

  // (2) DEWPOINT = F(Vapor Pressure)
	double T = log(VP/0.61078);   // temp var
	return (241.88 * T) / (17.558 - T);
}
//---------------------------------------------------------------

void getTempHumidity()
{
  tempc = htu.readTemperature();
  tempf = (tempc * 9)/5 + 32;

  humidity = htu.readHumidity();
}
//---------------------------------------------------------------
void getBaro()
{
  baroTemp = baro.readTempF();//get the temperature in F

  pascals = baro.readPressure();//get pressure in Pascals
  inches = pascals * 0.0002953;

  altf = baro.readAltitudeFt();//get altitude in feet
}
//---------------------------------------------------------------
void calcWeather()
{
  getTempHumidity();
  getBaro();

  //Calculate Dew Point
  dewptC = dewPoint(tempc, humidity);
  dewptF = (dewptC * 9.0)/ 5.0 + 32.0;
}
//---------------------------------------------------------------
