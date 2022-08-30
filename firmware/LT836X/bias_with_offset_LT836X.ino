#include <mcp47FEB22.h>
#include <Wire.h>
//#include <Adafruit_MCP4728.h>

#define TARGET_SAMD21 1 // toggle true samd21 target or arduino testing target

#if TARGET_SAMD21
// Proper at91samd21 build
#define DEVICE_NAME "SiPM 1.3 (at91samd21)"
#include <FlashAsEEPROM.h> // See lib FlashStorage for samd21/samd51 only
#else
// Generic Arduino build
#define DEVICE_NAME "SiPM 1.3 (arduino)"
#include <EEPROM.h> // Built-in lib
#endif

char NAME[] = DEVICE_NAME;

// DC/DC Booster ON/OFF gpio pin
#define BOOST_PIN 13
//#define HVLDO_PIN 12    //if LDO
#define HVLDO_PIN 7       //if LDO-LT836X

// 'factory' startup defaults // Based on our measurements of board
uint16_t gain = 3500, g42 = 3215, g50 = 3870; // Starting Gain Value, Gain Required for 42V, Gain Required for 50V
uint16_t offset = 220;
bool on = LOW;
int eeAddress = 0;
String cmd;
bool have_mcp = false;
char msgbuf[256];

//see "PRODUCT IDENTIFICATION SYSTEM", page 99 of the MCP manual
//MCP47FEB22AX where X = i2C address (e.g. MCP47FEB22A2) is address 2
uint8_t i2c_address = 2;
mcp47FEB22 mcp(i2c_address);

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(1000); // 1000ms timeout

  pinMode(BOOST_PIN, OUTPUT);
  //low disables the boost converter
  digitalWrite(BOOST_PIN, LOW);

  //high disables the LDO
  digitalWrite(HVLDO_PIN, HIGH);

  Wire.begin();

  delay(500);

  // Try to initialize!
  mcp.begin();  //no return value, so assume it was found
  have_mcp = true;
  if (have_mcp)
  {
    //1 = this channel uses internal Vref
    mcp.setVref(1, 1);
    //gain of 0 gives 2.048 volts maximum output
    mcp.setGain(0, 0);

    /* mcp.setChannelValue(MCP4728_CHANNEL_A, gain, MCP4728_VREF_INTERNAL,
                         MCP4728_GAIN_1X);
      mcp.setChannelValue(MCP4728_CHANNEL_B, offset, MCP4728_VREF_INTERNAL,
                         MCP4728_GAIN_1X);*/

    analogWriteResolution(10);
    //try to set the ADC to use the 1v internal reference since analogReference is for ADC only
    DAC->CTRLB.bit.REFSEL = 0;
  }
}


// Sets the output voltage using the input volt value and offset given
void setVoltageAndOffset(uint16_t volt, uint16_t offset)
{
  //Vout = 1.25+ (1.25/(48E3)+(1.25-V)/48E3)*1E6;
  mcp.analogWrite(4095 - volt, offset);


  //TODO:  set this sensibly
  //analogWrite(A0, lt8362_voltage);

  Serial.print("Analog Write:");
  Serial.print(volt);
  Serial.print(" / ");
  Serial.println(offset);
}



// Takes the input gain and will output a millivoltage it expects from that gain setting
uint16_t millivoltFromGain(uint16_t gain)
{
  // linear interpolation using 42000mV and 50000mV
  return (((long)gain - g42) * (8000L) + 42000L * ((long)g50 - g42)) / (g50 - g42);
}
// Takes input of millivolts and will return the gain expected to be required to generate that voltage
uint16_t gainFromVoltage(uint16_t millivolt)
{
  // linear interpolation using 42V and 50V
  return (((long)g50 - g42) * ((long)millivolt - 42000L) + g42 * 8000L) / (8000L);
}

uint16_t millivoltFromOffset(uint16_t o)
{
  // mcp level 2048 = 2048 mV, and actual opamp offset is 1/2
  return o / 2;
}

uint16_t offsetFromVoltage(uint16_t millivolt)
{
  // mcp level 2048 = 2048 mV, and actual opamp offset is 1/2
  return millivolt * 2;
}

void writeEEPROM()
{
  Serial.println("Writing ROM");
  // save offset value
  EEPROM.update(eeAddress, offset);
  EEPROM.update(eeAddress + 1, offset >> 8);
  //save calibration coefficients
  EEPROM.update(eeAddress + 2, g42);
  EEPROM.update(eeAddress + 3, g42 >> 8);
  EEPROM.update(eeAddress + 4, g50);
  EEPROM.update(eeAddress + 5, g50 >> 8);

#if TARGET_SAMD21
  EEPROM.commit();
#endif
}

void readEEPROM()
{
  Serial.println("Reading ROM");
  // Check if a sane value is present and if so use that as the offset
  uint16_t roffset = (EEPROM.read(eeAddress + 1) << 8) | (EEPROM.read(eeAddress) & 0xff);
  uint16_t rg42 = (EEPROM.read(eeAddress + 3) << 8) | (EEPROM.read(eeAddress + 2) & 0xff);
  uint16_t rg50 = (EEPROM.read(eeAddress + 5) << 8) | (EEPROM.read(eeAddress + 4) & 0xff);

  if (roffset > 2000 || roffset < 0)
  {
    sprintf(msgbuf, "eeprom invalid (offset value out of range: '%d')\n", roffset);
    Serial.print(msgbuf);
    return;
  }
  if (rg42 > 2000 || rg42 < 20)
  {
    sprintf(msgbuf, "eeprom invalid (g42 value out of range: '%d')\n", rg42);
    Serial.print(msgbuf);
    return;
  }
  if (rg50 > 2000 || rg50 < 20)
  {
    sprintf(msgbuf, "eeprom invalid (g50 value out of range: '%d')\n", rg50);
    Serial.print(msgbuf);
    return;
  }
  // all eeprom values valid and apply
  offset = roffset;
  setVoltageAndOffset(gain, offset);
  g42 = rg42;
  g50 = rg50;
}

void usage()
{
  Serial.println(NAME);
  Serial.println("Usage:");
  Serial.println("help[?]                 Return this message");
  Serial.println("name?                   Return device/firmware info");
  Serial.println("on[?]|off[?]            Set booster ON/OFF or query status");
  Serial.println("gain[?] level           Set gain level (0-4000) or query");
  Serial.println("offset[?] level         Set offset level (0-2048) or query");
  Serial.println("calibration[?] g42 g50  Set gain calibration levels at 42V and at 50V or query");
  Serial.println("[gain_]voltage[?] V    Set gain (bias) voltage [0 ~55] V or query, requires calibration");
  Serial.println("offset_voltage[?] mv    Set offset millivolt [0 2048] or query");
  Serial.println("read_rom                Recall defaults and calibration from ROM (startup values)");
  Serial.println("write_rom               Set gain and offset defaults, and calibration into ROM");
}

void parse_command(char *str)
{
  char *tok;

  tok = strtok(str, " \n");

  if (strcmp(tok, "on") == 0)
  {
    on = true;
    Serial.println("on"); // Always reply something
  }
  else if (strcmp(tok, "off") == 0)
  {
    on = false;
    Serial.println("off"); // Always reply something
  }
  else if (strcmp(tok, "on?") == 0 || strcmp(tok, "off?") == 0)
  {
    Serial.println(on ? "on" : "off");
  }
  else if (strcmp(tok, "gain?") == 0)
  {
    sprintf(msgbuf, "%d\n", gain);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "gain") == 0)
  {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4096 || g < 0)
      Serial.println("gain invalid (out of range [0 2000])");
    else
    {
      gain = (uint16_t)g; // valid range accepted
      setVoltageAndOffset(gain, offset);
    }

    // Always reply something:
    sprintf(msgbuf, "%d\n", gain);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "offset?") == 0)
  {
    sprintf(msgbuf, "%d\n", offset);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "offset") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o > 4096 || o < 0)
      Serial.println("offset invalid (out of range [0 4096])");
    else
    {
      offset = (uint16_t)o; // valid range accepted
      setVoltageAndOffset(gain, offset);
    }
    // Always reply something:
    sprintf(msgbuf, "%d\n", offset);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "gpioh") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o > 20 || o < 0)
      Serial.println("offset invalid (out of range [0 20])");
    else
    {
      digitalWrite(o, HIGH);
    }
    // Always reply something:
    sprintf(msgbuf, "set %d high\n", o);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "gpiol") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o > 20 || o < 0)
      Serial.println("offset invalid (out of range [0 20])");
    else
    {
      digitalWrite(o, LOW);
    }
    // Always reply something:
    sprintf(msgbuf, "set %d low\n", o);
    Serial.write(msgbuf);
  }

  else if (strcmp(tok, "dac") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o > 1023 || o < 0)
      Serial.println("offset invalid (out of range [0 1023])");
    else
    {
      //must write to A alias of the pin or its interpreted as digital
      analogWrite(A0, o);
    }
    // Always reply something:
    sprintf(msgbuf, "set dac A0 to %d\n", o);
    Serial.write(msgbuf);
  }

  else if (strcmp(tok, "offset_voltage?") == 0)
  {
    sprintf(msgbuf, "%d\n", millivoltFromOffset(offset));
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "offset_voltage") == 0)
  {
    tok = strtok(NULL, " \n");
    long millivolt = strtol(tok, NULL, 10);
    if (millivolt > 2048 || millivolt < 0)
      Serial.println("offset_voltage invalid (out of range [0 1024] mV)");
    else
    {
      offset = offsetFromVoltage(millivolt); // valid range accepted
      setVoltageAndOffset(gain, offset);

    }
    // Always reply something:
    sprintf(msgbuf, "%d\n", millivoltFromOffset(offset));
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "volts?") == 0 || strcmp(tok, "gain_voltage?") == 0)
  {
    sprintf(msgbuf, "%d\n", millivoltFromGain(gain));
    Serial.write(msgbuf);
  }
  // Change gain voltage input of milliVolts
  else if (strcmp(tok, "millivolts") == 0 || strcmp(tok, "gain_millivolts") == 0)
  {
    tok = strtok(NULL, " \n");
    long volts = strtol(tok, NULL, 10);
    uint16_t vmin = millivoltFromGain(g42);    // minimum millivolt depends on calibration
    uint16_t vmax = millivoltFromGain(4000); // maximum millivolt depeneds on calibration
    uint16_t g = gainFromVoltage(volts); // Has input of millivolts
    if (volts > vmax || volts < vmin)
    {
      sprintf(msgbuf, "voltage invalid (out of range [%d %d] mV\n", vmin, vmax);
      Serial.write(msgbuf);
    }
    else
    {
      gain = g; // valid range accepted
      setVoltageAndOffset(gain, offset);

    }
    // Always reply something:
    sprintf(msgbuf, "%d\n", millivoltFromGain(gain));
    Serial.write(msgbuf);
  }
  // Change gain voltage input of Volts
  else if (strcmp(tok, "voltage") == 0 || strcmp(tok, "gain_voltage") == 0)
  {
    tok = strtok(NULL, " \n");
    float inputVolt = strtol(tok, NULL, 10);
    long millivolts = inputVolt*1000; 
    float vmin = ((float) millivoltFromGain(g42))/1000;    // minimum millivolt depends on calibration
    float vmax = ((float) millivoltFromGain(4000))/1000; // maximum millivolt depeneds on calibration
    uint16_t g = gainFromVoltage(millivolts); // Has input of millivolts
    if (millivolts > vmax || millivolts < vmin)
    {
      sprintf(msgbuf, "voltage invalid (out of range [%d %d] V\n", vmin, vmax);
      Serial.write(msgbuf);
    }
    else
    {
      gain = g; // valid range accepted
      setVoltageAndOffset(gain, offset);

    }
    // Always reply something:
    sprintf(msgbuf, "%d\n", millivoltFromGain(gain));
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "calibration?") == 0)
  {
    sprintf(msgbuf, "Calibrated 42V to %d, 50V to %d\n", g42, g50);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "calibration") == 0)
  {
    // calibration gain@42v gain@50v
    tok = strtok(NULL, " \n");
    long tg42 = strtol(tok, NULL, 10);
    tok = strtok(NULL, " \n");
    long tg50 = strtol(tok, NULL, 10);
    if (tg42 > 4000 || tg42 <= 0)
    {
      Serial.println("calibration invalid (g42 out of range [0-4000])");
      return;
    }
    if (tg50 > 4000 || tg50 <= 0)
    {
      Serial.println("calibration invalid (g50 out of range [0-4000])");
      return;
    }
    // accept valid calibration coefficients
    g42 = tg42;
    g50 = tg50;
    sprintf(msgbuf, "Calibrated 42V to %d, 50V to %d\n", g42, g50);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "write_rom") == 0)
  {
    writeEEPROM();
  }
  else if (strcmp(tok, "read_rom") == 0)
  {
    readEEPROM();
  }
  else if (strcmp(tok, "help?") == 0 || strcmp(tok, "help") == 0)
  {
    usage();
  }
  else if (strcmp(tok, "?") == 0 || strcmp(tok, "name?") == 0)
  {
    Serial.println(NAME);
  }
  else if (strlen(tok) > 2)
  {
    sprintf(msgbuf, "Unknown command: '%s' (type help for a list of commands)\n", tok);
    Serial.write(msgbuf);
  }
}

int loopcount = 0;
void loop()
{
  if (loopcount < 100) {
    loopcount++;
    delay(100); // Count 100ms intervals until 10s.
  }
  if (loopcount < 30)  // Loop idle for 3s grace time.
    return;
  if (loopcount < 31) // At 3s mark: assume Serial is now working
  {
    // print init message
    if (!have_mcp)
      Serial.println("Failed to find MCP4728 chip");
    else
      Serial.println("Found MCP4728 chip");
    // apply settings from ROM
    //readEEPROM();
    setVoltageAndOffset(gain, offset);

  }
  // Listen for commands
  if (Serial.available())
  {
    cmd = Serial.readStringUntil('\n');
    char *str = (char *)cmd.c_str();
    parse_command(str);
  }
  // Update on/off bit
  delay(100);
  digitalWrite(BOOST_PIN, on);
  digitalWrite(HVLDO_PIN, !on);
}
