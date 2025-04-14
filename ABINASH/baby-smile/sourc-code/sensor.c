
#include "sSense_BME680.h" 
#define SERIAL_SPEED  19200
BME680_Class BME680; 


float altitude(const float seaLevel=1013.25) 
{

  static float Altitude;
  int32_t temp, hum, press, gas;
  BME680.getSensorData(temp,hum,press,gas); 
  Altitude = 44330.0*(1.0-pow(((float)press/100.0)/seaLevel,0.1903)); // Convert into altitude in meters
  return(Altitude);
} // of method altitude()

float calculate_altitude( float pressure, bool metric = true, float seaLevelPressure = 101325)
{
  /*Equations courtesy of NOAA - code ported from BME280*/;
  float altitude = NAN;
  if (!isnan(pressure) && !isnan(seaLevelPressure)){
    altitude = 1000.0 * ( seaLevelPressure - pressure ) / 3386.3752577878;
  }
  return metric ? altitude * 0.3048 : altitude;
}

float temperatureCompensatedAltitude(int32_t pressure, float temp=21.0 /*Celsius*/, float seaLevel=1013.25) 
{
  /*Casio equation - code written by itbrainpower.net*/
  float Altitude;
  Altitude = (pow((seaLevel/((float)pressure/100.0)), (1/5.257))-1)*(temp + 273.15) / 0.0065; // Convert into altitude in meters
  return(Altitude);	//this are metric value
} 



void setup()
{
  DebugPort.begin(SERIAL_SPEED); // Start serial port at Baud rate

  while(!DebugPort) {delay(10);} // Wait

  //delay(1000);

  DebugPort.println("s-Sense BME68x I2C sensor.");
  DebugPort.print("- Initializing BME68x sensor\n");
  while (!BME680.begin(I2C_STANDARD_MODE)) // Start BME68x using I2C protocol
  {
    DebugPort.println("-  Unable to find BME68x. Waiting 1 seconds.");
    delay(1000);
  } // of loop until device is located
  DebugPort.println("- Setting 16x oversampling for all sensors");
  BME680.setOversampling(TemperatureSensor,Oversample16); // Use enumerated type values
  BME680.setOversampling(HumiditySensor,   Oversample16);
  BME680.setOversampling(PressureSensor,   Oversample16);
  DebugPort.println("- Setting IIR filter to a value of 4 samples");
  BME680.setIIRFilter(IIR4);
  DebugPort.println("- Setting gas measurement to 320C for 150ms");
  BME680.setGas(320,150); // 320ï¿½c for 150 milliseconds
  DebugPort.println();
} // of method setup()


void loop() 
{
  //static uint8_t loopCounter = 0;
  static int32_t temperature, humidity, pressure, gas;     // Variable to store readings
  BME680.getSensorData(temperature,humidity,pressure,gas); // Get most recent readings
  DebugPort.print("\r\nSensor data >>\t\t");                       // Temperature in deci-degrees
  DebugPort.print(temperature/100.0,2);                       // Temperature in deci-degrees
  DebugPort.print("C\t");                          
  DebugPort.print(humidity/1000.0,2);                         // Humidity in milli-percent
  DebugPort.print("%\t");
  DebugPort.print(pressure/100.0,2);                          // Pressure in Pascals
  DebugPort.print("hPa\t");
  //DebugPort.print(pressure);                          // Pressure in Pascals
  //DebugPort.print("Pa ");
  DebugPort.print(gas/100.0,2);
  DebugPort.println("mOhm");

  DebugPort.println("\r\nCalculated altitude");

  DebugPort.print("temp comp [CASIO equation]: ");

  //temperatureCompensatedAltitude(int32_t pressure, float temp =21.0, const float seaLevel=1013.25)
  DebugPort.print(temperatureCompensatedAltitude(pressure, temperature/100.0/*, 1022.0*/),2); 
  DebugPort.print("m\t");


  DebugPort.print("NOAA equation: ");

  //float calculate_altitude( float pressure, bool metric = true, float seaLevelPressure = 101325)
  DebugPort.print(calculate_altitude((long)pressure,true),2); //calculate_altitude
  //DebugPort.print(calculate_altitude((long)pressure,true, (long)102200.0),2); //calculate_altitude
  DebugPort.print("m\t");

  DebugPort.print("WIKI equation: ");
  DebugPort.print(altitude(),2); 
  DebugPort.println("m \r\n");

  delay(1000);
} 
