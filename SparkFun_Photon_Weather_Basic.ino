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

float humidity = 0;
float tempf = 0;
float pascals = 0;
float altf = 0;
float baroTemp = 0;

unsigned long last = 0;

HTU21D htu = HTU21D();//create instance of HTU21D Temp and humidity sensor
MPL3115A2 baro = MPL3115A2();//create instance of MPL3115A2 barrometric sensor

//---------------------------------------------------------------
void setup()
{
  Serial.begin(9600);   // open serial over USB at 9600 baud

  //Initialize both on-board sensors
  //Initialize both on-board sensors
  while(! htu.begin()){
    Serial.println("HTU21D not found");
    delay(1000);
  }
  Serial.println("HTU21D OK");

  while(! baro.begin()) {
    Serial.println("MPL3115A2 not found");
    delay(1000);
  }
  Serial.println("MPL3115A2 OK");

  //MPL3115A2 Settings
  //baro.setModeBarometer();//Set to Barometer Mode
  baro.setModeAltimeter();//Set to altimeter Mode

  baro.setOversampleRate(7); // Set Oversample to the recommended 128
  baro.enableEventFlags(); //Necessary register calls to enble temp, baro ansd alt
}
//---------------------------------------------------------------
void loop()
{
  //Get readings from all sensors
  calcWeather();
  //Rather than use a delay, keeping track of a counter allows the photon to
  //still take readings and do work in between printing out data.
  unsigned long now = millis();
  //alter this number to change the amount of time between each reading
  if(now - last > 10000)//prints roughly every 10 seconds
  {
    last = now;
    printInfo();
    sendInfo();
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
  d.concat((tempf+baroTemp)/2);
  d.concat(",");
  d.concat("\"humidity\":");
  d.concat(humidity);
  d.concat(",");
  d.concat("\"pressure\":");
  d.concat(pascals);
  d.concat(",");
  d.concat("\"altitude\":");
  d.concat(altf);
  d.concat("}");
  Serial.println(d);
  Spark.publish("weather.test.01", d, 60, PRIVATE);
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
//---------------------------------------------------------------
void getTempHumidity()
{
  float temp = 0;

  temp = htu.readTemperature();
  tempf = (temp * 9)/5 + 32;

  humidity = htu.readHumidity();
}
//---------------------------------------------------------------
void getBaro()
{
  baroTemp = baro.readTempF();//get the temperature in F

  pascals = baro.readPressure();//get pressure in Pascals

  altf = baro.readAltitudeFt();//get altitude in feet
}
//---------------------------------------------------------------
void calcWeather()
{
  getTempHumidity();
  getBaro();
}
//---------------------------------------------------------------
