#define RS845_DEFAULT_DE_PIN 3
#define RS845_DEFAULT_RE_PIN -1

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Ticker.h"
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>
#include <EEPROM.h>

#define PANEL1_POWER_SUPPLY_PIN A3
#define PANEL2_POWER_SUPPLY_PIN A4

#define MODBUS_DEFAULT_DEVICE_ADDRESS 255
#define MODBUS_TEMP_SENSORS_START_ADDRESS 100
#define MODBUS_COILS_START_ADDRESS 100

#define DS18_RESOLUTION 12
#define ONE_WIRE_BUS 2
#define STATUS_LED LED_BUILTIN
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature temperatureSensors(&oneWire);

uint8_t temperatureSensorsCount = 0;
uint8_t modbusDeviceAddress;

void chargePanel1(bool on = true);
void chargePanel2(bool on = true);
uint8_t getModbusUnitAddress();
void saveModbusUnitAddress(uint8_t address);
void readTemperaturesCallback();

Ticker readTemperaturesTicker(readTemperaturesCallback, 750 / (1 << (12 - DS18_RESOLUTION)), 0l, MILLIS);

void (*resetFunc)(void) = 0;

union
{
  float value;
  struct
  {
    uint16_t lowOrderByte;
    uint16_t highOrderByte;
  };
} temperatureUnion;

void setup()
{
  pinMode(PANEL1_POWER_SUPPLY_PIN, OUTPUT);
  pinMode(PANEL2_POWER_SUPPLY_PIN, OUTPUT);
  chargePanel1(false);
  chargePanel2(false);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);
  _delay_ms(3000);
  digitalWrite(STATUS_LED, LOW);

  // for (int i = 0; i < EEPROM.length(); i++)
  // {
  //   EEPROM.write(i, 0);
  // }
  modbusDeviceAddress = getModbusUnitAddress();
  if (!ModbusRTUServer.begin(modbusDeviceAddress, 9600))
  {
    while (1)
      ;
  }

  temperatureSensors.begin();
  temperatureSensors.setWaitForConversion(false);
  temperatureSensors.setResolution(DS18_RESOLUTION);
  temperatureSensors.requestTemperatures();
  temperatureSensorsCount = temperatureSensors.getDS18Count();

  ModbusRTUServer.configureHoldingRegisters(100, 1);
  ModbusRTUServer.holdingRegisterWrite(100, modbusDeviceAddress);

  ModbusRTUServer.configureCoils(MODBUS_COILS_START_ADDRESS, 2);
  ModbusRTUServer.configureInputRegisters(MODBUS_TEMP_SENSORS_START_ADDRESS, temperatureSensorsCount * 2);

  readTemperaturesTicker.start();
}

void loop()
{
  readTemperaturesTicker.update();
  ModbusRTUServer.poll();

  uint8_t devAddress = ModbusRTUServer.holdingRegisterRead(100);
  if (devAddress != modbusDeviceAddress)
  {
    saveModbusUnitAddress(devAddress);
    resetFunc();
  }

  for (uint8_t coil = 0; coil < 2; coil++)
  {
    int coilValue = ModbusRTUServer.coilRead(MODBUS_COILS_START_ADDRESS + coil);
    if (coil == 0)
    {
      chargePanel1(coilValue);
    }
    if (coil == 1)
    {
      chargePanel2(coilValue);
    }
  }
}

void chargePanel1(bool on = true)
{
  digitalWrite(PANEL1_POWER_SUPPLY_PIN, !on);
}
void chargePanel2(bool on = true)
{
  digitalWrite(PANEL2_POWER_SUPPLY_PIN, !on);
}

void readTemperaturesCallback()
{

  for (uint8_t i = 0; i < temperatureSensorsCount; i++)
  {
    temperatureUnion.value = temperatureSensors.getTempCByIndex(0);
    ModbusRTUServer.inputRegisterWrite(MODBUS_TEMP_SENSORS_START_ADDRESS + (i * 2), temperatureUnion.highOrderByte);
    ModbusRTUServer.inputRegisterWrite(MODBUS_TEMP_SENSORS_START_ADDRESS + (i * 2 + 1), temperatureUnion.lowOrderByte);
  }

  temperatureSensors.requestTemperatures();
}

uint8_t getModbusUnitAddress()
{
  uint8_t address;
  EEPROM.get(0, address);
  if (address <= 0)
  {
    return MODBUS_DEFAULT_DEVICE_ADDRESS;
  }
  return address;
}

void saveModbusUnitAddress(uint8_t address)
{
  EEPROM.update(0, address);
}