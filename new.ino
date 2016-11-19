/******************************************************************************
  SparkFun_Photon_Weather_Wunderground.ino
  SparkFun Photon Weather Shield basic example
  Joel Bartlett @ SparkFun Electronics
  Original Creation Date: May 18, 2015
  Updated August 21, 2015
  This sketch prints the temperature, humidity, and barometric pressure OR
  altitude to the Serial port.

  The library used in this example can be found here:
  https://github.com/sparkfun/SparkFun_Photon_Weather_Shield_Particle_Library

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

//---------------------------------------------------------------

  Weather Underground Upload sections: Dan Fein @ Weather Underground
  Weather Underground Upload Protocol:
  http://wiki.wunderground.com/index.php/PWS_-_Upload_Protocol
  Sign up at http://www.wunderground.com/personal-weather-station/signup.asp

*******************************************************************************/
#include "SparkFun_Photon_Weather_Shield_Library.h"
#include "math.h"   //For Dew Point Calculation

PRODUCT_ID(764);
PRODUCT_VERSION(1);

//Create Instance of HTU21D or SI7021 temp and humidity sensor and MPL3115A2 barometric sensor
Weather sensor;

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// digital I/O pins
const byte WSPEED = 3;
const byte RAIN = 2;

// analog I/O pins
const byte REFERENCE_3V3 = A3;
const byte BATT = A2;
const byte WDIR = A0;
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
long lastSecond; //The millis counter to see when a second rolls by
unsigned int minutesSinceLastReset; //Used to reset variables after 24 hours. Imp should tell us when it's midnight, this is backup.
byte seconds; //When it hits 60, increase the current minute
byte seconds_2m; //Keeps track of the "wind speed/dir avg" over last 2 minutes array of data
byte minutes; //Keeps track of where we are in various arrays of data
byte minutes_10m; //Keeps track of where we are in wind gust/dir over last 10 minutes array of data

long lastWindCheck = 0;
volatile long lastWindIRQ = 0;
volatile byte windClicks = 0;

//We need to keep track of the following variables:
//Wind speed/dir each update (no storage)
//Wind gust/dir over the day (no storage)
//Wind speed/dir, avg over 2 minutes (store 1 per second)
//Wind gust/dir over last 10 minutes (store 1 per minute)
//Rain over the past hour (store 1 per minute)
//Total rain over date (store one per day)

byte windspdavg[120]; //120 bytes to keep track of 2 minute average

#define WIND_DIR_AVG_SIZE 120
int winddiravg[WIND_DIR_AVG_SIZE]; //120 ints to keep track of 2 minute average
float windgust_10m[10]; //10 floats to keep track of 10 minute max
int windgustdirection_10m[10]; //10 ints to keep track of 10 minute max
volatile float rainHour[60]; //60 floating numbers to keep track of 60 minutes of rain

//These are all the weather values that wunderground expects:
int winddir = 0; // [0-360 instantaneous wind direction]
float windspeedmph = 0; // [mph instantaneous wind speed]
float windgustmph = 0; // [mph current wind gust, using software specific time period]
int windgustdir = 0; // [0-360 using software specific time period]
float windspdmph_avg2m = 0; // [mph 2 minute average wind speed mph]
int winddir_avg2m = 0; // [0-360 2 minute average wind direction]
float windgustmph_10m = 0; // [mph past 10 minutes wind gust mph ]
int windgustdir_10m = 0; // [0-360 past 10 minutes wind gust direction]
float humidity = 0; // [%]
float tempF = 0; // [temperature F]
float tempC = 0;     //Average of the sensors temperature readings, celsius
float rainin = 0; // [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
volatile float dailyrainin = 0; // [rain inches so far today in local time]
//float baromin = 30.03;// [barom in] - It's hard to calculate baromin locally, do this in the agent
float pressure = 0;
//float dewptf; // [dewpoint F] - It's hard to calculate dewpoint locally, do this in the agent
float humTempF = 0;  //humidity sensor temp reading, fahrenheit
float humTempC = 0;  //humidity sensor temp reading, celsius
float baroTempF = 0; //barometer sensor temp reading, fahrenheit
float baroTempC = 0; //barometer sensor temp reading, celsius
float dewptF = 0;
float dewptC = 0;
float pascals = 0;
float inches = 0;

float batt_lvl = 11.8; //[analog value from 0 to 1023]

// volatiles are subject to modification by IRQs
volatile unsigned long raintime, rainlast, raininterval, rain;

int timeSynced = 0;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Interrupt routines (these are called by the hardware interrupts, not by the main code)
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
void rainIRQ()
// Count rain gauge bucket tips as they occur
// Activated by the magnet and reed switch in the rain gauge, attached to input D2
{
	raintime = millis(); // grab current time
	raininterval = raintime - rainlast; // calculate interval between this and last event

	if (raininterval > 10) // ignore switch-bounce glitches less than 10mS after initial edge
	{
		dailyrainin += 0.011; //Each dump is 0.011" of water
		rainHour[minutes] += 0.011; //Increase this minute's amount of rain

		rainlast = raintime; // set up for next event
	}
}

void wspeedIRQ()
// Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
{
	if (millis() - lastWindIRQ > 10) // Ignore switch-bounce glitches less than 10ms (142MPH max reading) after the reed switch closes
	{
		lastWindIRQ = millis(); //Grab the current time
		windClicks++; //There is 1.492MPH for each click per second.
	}
}

//---------------------------------------------------------------
void setup()
{
  WiFi.selectAntenna(ANT_AUTO);

  pinMode(WSPEED, INPUT_PULLUP); // input from wind meters windspeed sensor
  pinMode(RAIN, INPUT_PULLUP); // input from wind meters rain gauge sensor

  //Initialize the I2C sensors and ping them
  sensor.begin();

  /*You can only receive acurate barrometric readings or acurate altitiude
    readings at a given time, not both at the same time. The following two lines
    tell the sensor what mode to use. You could easily write a function that
    takes a reading in one made and then switches to the other mode to grab that
    reading, resulting in data that contains both acurate altitude and barrometric
    readings. For this example, we will only be using the barometer mode. Be sure
    to only uncomment one line at a time. */
  sensor.setModeBarometer();//Set to Barometer Mode
  //baro.setModeAltimeter();//Set to altimeter Mode

  //These are additional MPL3115A2 functions the MUST be called for the sensor to work.
  sensor.setOversampleRate(7); // Set Oversample rate
  //Call with a rate from 0 to 7. See page 33 for table of ratios.
  //Sets the over sample rate. Datasheet calls for 128 but you can set it
  //from 1 to 128 samples. The higher the oversample rate the greater
  //the time between data samples.

  sensor.enableEventFlags(); //Necessary register calls to enble temp, baro ansd alt

  // attach external interrupt pins to IRQ functions
  attachInterrupt(0, rainIRQ, FALLING);
  attachInterrupt(1, wspeedIRQ, FALLING);

  // turn on interrupts
  interrupts();

  delay(10000);
}

//---------------------------------------------------------------
void loop()
{

	//Keep track of which minute it is
  if(millis() - lastSecond >= 1000)
	{

    lastSecond += 1000;

		//Take a speed and direction reading every second for 2 minute average
		if(++seconds_2m > 119) seconds_2m = 0;

		//Calc the wind speed and direction every second for 120 second to get 2 minute average
		float currentSpeed = windspeedmph;
		//float currentSpeed = random(5); //For testing
		int currentDirection = get_wind_direction();
		windspdavg[seconds_2m] = (int)currentSpeed;
		winddiravg[seconds_2m] = currentDirection;
		//if(seconds_2m % 10 == 0) displayArrays(); //For testing

		//Check to see if this is a gust for the minute
		if(currentSpeed > windgust_10m[minutes_10m])
		{
			windgust_10m[minutes_10m] = currentSpeed;
			windgustdirection_10m[minutes_10m] = currentDirection;
		}

		//Check to see if this is a gust for the day
		if(currentSpeed > windgustmph)
		{
			windgustmph = currentSpeed;
			windgustdir = currentDirection;
		}

		if(++seconds > 59)
		{
			seconds = 0;

			if(++minutes > 59) minutes = 0;
			if(++minutes_10m > 9) minutes_10m = 0;

			rainHour[minutes] = 0; //Zero out this minute's rainfall amount
			windgust_10m[minutes_10m] = 0; //Zero out this minute's gust
		}

    if (Time.hour() == 0 && timeSynced == 0) {
      Particle.syncTime();
      timeSynced = 1;
    }

    if (Time.hour() == 1) {
      timeSynced = 0;
    }

    calcWeather();
    publishWeather();

	}

  delay(60000);
      //Power down between sends to save power, measured in seconds.
      //System.sleep(SLEEP_MODE_DEEP,60);  //for Particle Photon
      // delay(300);                         //Without the delay it goes to sleep too fast and the send is unreliable

}

void publishWeather() {
  String x = "";
  x = String(x + "tf=");
  x = String(x + String(tempF));
  x = String(x + "&df=");
  x = String(x + String(dewptF));
  x = String(x + "&h=");
  x = String(x + String(humidity));
  x = String(x + "&b=");
  x = String(x + String(inches));
  x = String(x + "&wd=");
  x = String(x + String(winddir));
  x = String(x + "&wsm=");
  x = String(x + String(windspeedmph));
  x = String(x + "&wgd=");
  x = String(x + String(windgustdir));
  x = String(x + "&wsm2=");
  x = String(x + String(windspdmph_avg2m));
  x = String(x + "&wd2=");
  x = String(x + String(winddir_avg2m));
  x = String(x + "&wgm10=");
  x = String(x + String(windgustmph_10m));
  x = String(x + "&wgd10=");
  x = String(x + String(windgustdir_10m));
  x = String(x + "&ri=");
  x = String(x + String(rainin));
  x = String(x + "&dri=");
  x = String(x + String(dailyrainin));
  Particle.publish("weather", x, PRIVATE);
}

//Calculates each of the variables that wunderground is expecting
void calcWeather()
{
	//Calc winddir
	winddir = get_wind_direction();

	//Calc windspeed
	windspeedmph = get_wind_speed();

	//Calc windgustmph
	//Calc windgustdir
	//These are calculated in the main loop

	//Calc windspdmph_avg2m
	float temp = 0;
	for(int i = 0 ; i < 120 ; i++)
		temp += windspdavg[i];

	temp /= 120.0;
	windspdmph_avg2m = temp;

	//Calc winddir_avg2m, Wind Direction
	//You can't just take the average. Google "mean of circular quantities" for more info
	//We will use the Mitsuta method because it doesn't require trig functions
	//And because it sounds cool.
	//Based on: http://abelian.org/vlf/bearings.html
	//Based on: http://stackoverflow.com/questions/1813483/averaging-angles-again
	long sum = winddiravg[0];
	int D = winddiravg[0];
	for(int i = 1 ; i < WIND_DIR_AVG_SIZE ; i++)
	{
		int delta = winddiravg[i] - D;

		if(delta < -180)
			D += delta + 360;
		else if(delta > 180)
			D += delta - 360;
		else
			D += delta;

		sum += D;
	}
	winddir_avg2m = sum / WIND_DIR_AVG_SIZE;
	if(winddir_avg2m >= 360) winddir_avg2m -= 360;
	if(winddir_avg2m < 0) winddir_avg2m += 360;

	//Calc windgustmph_10m
	//Calc windgustdir_10m
	//Find the largest windgust in the last 10 minutes
	windgustmph_10m = 0;
	windgustdir_10m = 0;
	//Step through the 10 minutes
	for(int i = 0; i < 10 ; i++)
	{
		if(windgust_10m[i] > windgustmph_10m)
		{
			windgustmph_10m = windgust_10m[i];
			windgustdir_10m = windgustdirection_10m[i];
		}
	}

	//Total rainfall for the day is calculated within the interrupt
	//Calculate amount of rainfall for the last 60 minutes
	rainin = 0;
	for(int i = 0 ; i < 60 ; i++)
		rainin += rainHour[i];

  // Measure Relative Humidity from the HTU21D or Si7021
  humidity = sensor.getRH();

  // Measure Temperature from the HTU21D or Si7021
  humTempC = sensor.getTemp();
  humTempF = (humTempC * 9)/5 + 32;
  // Temperature is measured every time RH is requested.
  // It is faster, therefore, to read it from previous RH
  // measurement with getTemp() instead with readTemp()

  //Measure the Barometer temperature in F from the MPL3115A2
  baroTempC = sensor.readBaroTemp();
  baroTempF = (baroTempC * 9)/5 + 32; //convert the temperature to F

  //Measure Pressure from the MPL3115A2
  pascals = sensor.readPressure();
  inches = pascals * 0.0002953; // Calc for converting Pa to inHg (Wunderground expects inHg)

  //If in altitude mode, you can get a reading in feet with this line:
  //float altf = sensor.readAltitudeFt();

  //Average the temperature reading from both sensors
  tempC=((humTempC+baroTempC)/2);
  tempF=((humTempF+baroTempF)/2);

  //Calculate Dew Point
  dewptC = dewPoint(tempC, humidity);
  dewptF = (dewptC * 9.0)/ 5.0 + 32.0;
}

//Returns the instataneous wind speed
float get_wind_speed()
{
	float deltaTime = millis() - lastWindCheck; //750ms

	deltaTime /= 1000.0; //Covert to seconds

	float windSpeed = (float)windClicks / deltaTime; //3 / 0.750s = 4

	windClicks = 0; //Reset and start watching for new wind
	lastWindCheck = millis();

	windSpeed *= 1.492; //4 * 1.492 = 5.968MPH

	return(windSpeed);
}

//Read the wind direction sensor, return heading in degrees
int get_wind_direction()
{
	unsigned int adc;

	adc = analogRead(WDIR); // get the current reading from the sensor

	// The following table is ADC readings for the wind direction sensor output, sorted from low to high.
	// Each threshold is the midpoint between adjacent headings. The output is degrees for that ADC reading.
	// Note that these are not in compass degree order! See Weather Meters datasheet for more information.

	if (adc < 380) return (113);
	if (adc < 393) return (68);
	if (adc < 414) return (90);
	if (adc < 456) return (158);
	if (adc < 508) return (135);
	if (adc < 551) return (203);
	if (adc < 615) return (180);
	if (adc < 680) return (23);
	if (adc < 746) return (45);
	if (adc < 801) return (248);
	if (adc < 833) return (225);
	if (adc < 878) return (338);
	if (adc < 913) return (0);
	if (adc < 940) return (293);
	if (adc < 967) return (315);
	if (adc < 990) return (270);
	return (-1); // error, disconnected?
}

//When the imp tells us it's midnight, reset the total amount of rain and gusts
void midnightReset()
{
	dailyrainin = 0; //Reset daily amount of rain

	windgustmph = 0; //Zero out the windgust for the day
	windgustdir = 0; //Zero out the gust direction for the day

	minutes = 0; //Reset minute tracker
	seconds = 0;
	lastSecond = millis(); //Reset variable used to track minutes

	minutesSinceLastReset = 0; //Zero out the backup midnight reset variable
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
