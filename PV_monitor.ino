/*
PV monitoring

bus voltage range = 0 - 36V
current range = +/- 500mA -  +/- 50A depend on shunt resistor

WARNING!
1.  Please pay attention to any written warnings to avoid damage to the device! 
2.  Don't use USB cable and power supply at the same time! If you want to check if the measurement system is 
    work properly or not, YOU MUST UNPLUG ANY POWER SUPPLY THAT CONNECTED TO THIS MEASUREMENT SYSTEM and connect it to PC 
    using the USB cable and then open the serial monitor from arduino IDE. 
3.  Turn on the device first before connecting it to the PV panel, and when you are going to 
    turn off/end the measurement, first disconnect the PV panel from this device, and then
    turn it off.

used pin and I2C address by each sensors
1. SEN0390: digital 6, and 5 (6 = scl, 5 = sda) 
2. DS18B20: digital 4 
3. INA226: scl, sda (i2c address 0x40 and 0x44)
*/

#include <Wire.h>

/*DS1307 RTC*/
#include <TimeLib.h>
#include <DS1307RTC.h>
tmElements_t tm;

/*luxmeter SEN0390*/
#include <DFRobot_B_LUX_V30B.h>
DFRobot_B_LUX_V30B myLux(13,6,5); // pin used by SEN0390: scl = 6, sda = 5
long lux; //store raw value from lux meter 
long luxCal; //store calibrated value from lux meter

/*Temp sensor DS18B20*/
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4 //DS18B20 data = 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int tempC1, tempC2; //store temp sensor 1 and 2 value respectively in integer
float ftemp1, ftemp2; // store temp sensor 1 and 2 value respectively in float 
float calTemp1; //store calibrated temp value of temp sensor 1 value
float calTemp2; //store calibrated temp value of temp sensor 2 value

/*round the decimal*/
float dec1; 
float dec2;
int r1;
int r2;

/*SDcard*/
#include <SPI.h>
#include <SD.h>
File myFile;

/*INA226*/
#include <INA226_WE.h>
#define INA1_ADDRESS 0x40
#define INA2_ADDRESS 0x44

INA226_WE ina226_1 = INA226_WE(INA1_ADDRESS);
INA226_WE ina226_2 = INA226_WE(INA2_ADDRESS);

float busVoltage_V_1, current_mA_1, pwr_mW_1; //INA226_1 measured parameter
float busVoltage_V_2, current_mA_2, pwr_mW_2; //INA226_2 measured parameter


void setup() {
  Serial.begin(9600);
  Wire.begin();

  while (!Serial);


  Serial.print("Initializing SD card...");

  if (!SD.begin(10)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  if (SD.exists("data.txt")) {
    Serial.println("data.txt exists.");
    myFile = SD.open("data.txt", FILE_WRITE);
    myFile.close();
  } else {
    Serial.println("Creating data.txt...");
    myFile = SD.open("data.txt", FILE_WRITE);
    myFile.println("date,time,temp_1,temp_2,lux,V1(V),I1(mA),P1(mW),V2(V),I2(mA),P2(mW)");
    myFile.close();
  }

  /*DS18B20-setup*/
  sensors.begin();  
  sensors.setResolution(10);
  calTemp1=0; //store calibrated temp value of temp sensor 1 value
  calTemp2=0; //store calibrated temp value of temp sensor 2 value
  temp_read();

  /*SEN0390*/
  myLux.begin();

  /*INA226*/
  ina226_1.waitUntilConversionCompleted(); 
  ina226_2.waitUntilConversionCompleted();

  if(!ina226_1.init()){
    Serial.println("Failed to init INA226_1. Check your wiring.");
    while(1){}
  }

  if(!ina226_2.init()){
    Serial.println("Failed to init INA226_2. Check your wiring.");
    while(1){}
  }
}

void loop() {
  Serial.begin(9600);
  rtc_read();
}

void rtc_read(){
  
  if (RTC.read(tm)) {
    if(tm.Second == 0){
      /*Date*/
      Serial.print(tm.Day);
      Serial.write('/');
      Serial.print(tm.Month);
      Serial.write('/');
      Serial.print(tmYearToCalendar(tm.Year));
      Serial.print(",");
      /*Time*/
      print2digits(tm.Hour);
      Serial.write(':');
      print2digits(tm.Minute);
      Serial.write(':');
      print2digits(tm.Second);
      Serial.print(",");
      /*Temperature*/
      temp_read();
      Serial.print(calTemp1);
      Serial.print(",");
      Serial.print(calTemp2);
      Serial.print(",");
      /*Lux*/
      lux_read();
      Serial.print(luxCal);
      Serial.print(",");
      /*INA226 voltage and current*/ 
      ina226_read();
      Serial.print(busVoltage_V_1);
      Serial.print(","); 
      Serial.print(current_mA_1);
      Serial.print(","); 
      Serial.print(busVoltage_V_1 * current_mA_1); //power
      Serial.print(","); 
      Serial.print(busVoltage_V_2);
      Serial.print(","); 
      Serial.print(current_mA_2);
      Serial.print(","); 
      Serial.println(busVoltage_V_2 * current_mA_2); //power
      sd_log();
    }
  } else {
    if (RTC.chipPresent()) {
      Serial.println("The DS1307 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();
    } else {
      Serial.println("DS1307 read error!  Please check the circuitry.");
      Serial.println();
    }
    delay(9000);
  }
  delay(1000);
}

void sd_log(){
  myFile = SD.open("data.txt", FILE_WRITE);
  /*Date*/
  myFile.print(tm.Day);
  myFile.print("/");
  myFile.print(tm.Month);
  myFile.print("/");
  myFile.print(tmYearToCalendar(tm.Year));
  myFile.print(",");
  /*Time*/
  myFile.print(tm.Hour);
  myFile.print(":");
  myFile.print(tm.Minute);
  myFile.print(":");
  myFile.print(tm.Second);
  myFile.print(",");
  /*Temperature*/
  myFile.print(calTemp1);
  myFile.print(",");
  myFile.print(calTemp2);
  myFile.print(",");
  /*Lux*/
  myFile.print(luxCal);
  myFile.print(",");
  /*INA226 voltage and current*/
  myFile.print(busVoltage_V_1);
  myFile.print(","); 
  myFile.print(current_mA_1);
  myFile.print(","); 
  myFile.print(busVoltage_V_1 * current_mA_1);
  myFile.print(","); 
  myFile.print(busVoltage_V_2);
  myFile.print(",");
  myFile.print(current_mA_2);
  myFile.print(","); 
  myFile.println(busVoltage_V_2 * current_mA_2);
  myFile.close();
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

void temp_read(){
  sensors.requestTemperatures();
  tempC1 = sensors.getTempCByIndex(0); //read temperature of temp sensor 1 in integer
  tempC2 = sensors.getTempCByIndex(1); //read temperature of temp sensor 2 in integer
  ftemp1 = sensors.getTempCByIndex(0); //read temperature of temp sensor 1 in float
  ftemp2 = sensors.getTempCByIndex(1); //read temperature of temp sensor 2 in float

  // /*round the decimal*/
  dec1 = ftemp1-tempC1; 
  dec2 = ftemp2-tempC2;
  

  if(dec1<0.5){
    r1 = tempC1;
  }else if(dec1>=0.5){
    r1 = tempC1+1;
  }
  
  if(dec2<0.5){
    r2 = tempC2;
  }else if(dec2>=0.5){
    r2 = tempC2+1;
  }

  calTemp1 = r1; //for temp sensor 1 calibrated value
  calTemp2 = r2; //for temp sensor 2 calibrated value
}

void lux_read(){
  lux = myLux.lightStrengthLux();
  luxCal = lux; //for luxmeter calibrated value
  if (lux<0){
    lux = 0;
  }
  if (luxCal<0){
    luxCal = 0;
  }
}

void ina226_read(){
  
  busVoltage_V_1 = 0.0;
  current_mA_1 = 0.0;
  pwr_mW_1 = 0.0; 

  
  busVoltage_V_1 = ina226_1.getBusVoltage_V();
  current_mA_1 = ina226_1.getCurrent_mA();
  

  
  busVoltage_V_2 = 0.0;
  current_mA_2 = 0.0;
  pwr_mW_2 = 0.0;

  
  busVoltage_V_2 = ina226_2.getBusVoltage_V();
  current_mA_2 = ina226_2.getCurrent_mA();
}

