#include <mcp47FEB22.h>
#include <Wire.h>
//#include <Adafruit_MCP4728.h>

#define TARGET_SAMD21 1 // toggle true samd21 target or arduino testing target

#if TARGET_SAMD21
// Proper at91samd21 build
#define DEVICE_NAME "SiPM 1.32 (at91samd21)"
#include <FlashAsEEPROM.h> // See lib FlashStorage for samd21/samd51 only
#else
// Generic Arduino build
#define DEVICE_NAME "SiPM 1.32 (arduino)"
#include <EEPROM.h> // Built-in lib
#endif

char NAME[] = DEVICE_NAME;

// DC/DC Booster ON/OFF gpio pin
#define BOOST_PIN 13
#define HVLDO_PIN 7       //if LDO-LT836X

// 'factory' startup defaults // Based on our measurements of board
uint16_t gain = 3500, g42 = 2538, g50 = 3063; // Starting Gain Value, Gain Required for 42V, Gain Required for 50V
uint16_t offset = 220;
uint16_t lt8361_voltage = 418;  //default voltage for the boost converter, 418 = 60V, 81 = 64V
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
void setVoltageAndOffset(uint16_t gain, uint16_t offset)
{
  //Vout = 1.25+ (1.25/(48E3)+(1.25-V)/48E3)*1E6;
  mcp.analogWrite(4095 - gain, offset);

  Serial.print("Analog Write:");
  Serial.print(gain);
  Serial.print(" / ");
  Serial.println(offset);
}



// Takes the input gain and will output a millivoltage it expects from that gain setting
uint32_t millivoltFromGain(uint16_t gain)
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

uint32_t millivoltFromOffset(uint16_t o)
{
  // mcp level 2048 = 2048 mV, and actual opamp offset is 1/2
  return o / 2;
}

uint16_t offsetFromVoltage(uint32_t millivolt)
{
  // mcp level 2048 = 2048 mV, and actual opamp offset is 1/2
  return millivolt * 2;
}

//calculate the actual output voltage (in mv) for the LT8361 using the default feedback network
uint32_t lt8361_voltage_millivolts(uint32_t dn)
{
  //do calculation in uV to avoid floating point
  //return (1.6E6  + (1.6E6/39.0E3 + (1.6E6 - dn*1E6/1024*3.3) / 69.8E3))/1E6;  //from ohm's law
  //return (1.6E6  + ((1.6E6/39.0E3 + 1.6E6/ 69.8E3) - dn*1E6/1024*3.3/69.8E3))/1E6;  //from ohm's law

  //return (1600000  + (1600000/39000 + (1600000 - dn*3300000/1024) / 69800)*1000000)/1000;
  return (1600000  + ((1600000/39000 + 1600000/69800) - dn*3300000/1024/ 69800 )*1000000)/1000;
}

//calculate the actual output voltage (in mv) for the LT8361 using the default feedback network
uint32_t lt8361_dn_from_millivolts(uint32_t mv)
{
  //do calculation in uV to avoid floating point
  //dn = (mv - 65548)/(-46.1698); //inverse fit to the above function
  uint32_t dn = (mv*1000-65548000)/-46170;
  return dn;
  
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
  if (rg42 > 4000 || rg42 < 20)
  {
    sprintf(msgbuf, "eeprom invalid (g42 value out of range: '%d')\n", rg42);
    Serial.print(msgbuf);
    return;
  }
  if (rg50 > 4000 || rg50 < 20)
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
  Serial.println("gain[?] level           Set gain level (0-4095) or query");
  Serial.println("offset[?] level         Set offset level (0-2047) or query");
  Serial.println("calibration[?] g42 g50  Set gain calibration levels at 42V and at 50V or query");
  Serial.println("voltage[?] V            Set gain (bias) voltage [5 52.3] V or query, requires calibration, voltage must be < lt836x_voltage - 250mV");
  Serial.println("millivolts[?] mV        Set gain (bias) voltage in units of millivolts");
  Serial.println("gain[?] DN              Set gain (bias) DNs [0 4095] or query");
  Serial.println("offset_voltage[?] DN    Set offset DNs [0 2047] or query");
  Serial.println("lt8361_voltage[?] DN    Set boost converter voltage DN [0 1023] mapping to [65.5V 18V], default 60V on LT8361");
  Serial.println("                        Note:  boost converter voltage should be at least 1V greater than bias voltage");
  Serial.println("read_rom                Recall defaults and calibration from ROM (startup values)");
  Serial.println("write_rom               Set gain and offset defaults, and calibration into ROM");
}

void parse_command(char *str)
{
  char *tok;

  tok = strtok(str, " \n");

  if (strcmp(tok, "on") == 0)
  {
    //configure the LT836x output voltage using the arduino DAC
    analogWrite(A0, lt8361_voltage);
    
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
  //set bias voltage in DAC units
  else if (strcmp(tok, "gain") == 0)
  {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4096 || g < 0)
      Serial.println("gain invalid (out of range [0 4095])");
    else
    {
      gain = (uint16_t)g; // valid range accepted
      setVoltageAndOffset(gain, offset);
    }

    // Always reply something:
    sprintf(msgbuf, "%d dn and voltage %u\n", gain, lt8361_voltage_millivolts(gain));
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "offset?") == 0)
  {
    sprintf(msgbuf, "%d\n", offset);
    Serial.write(msgbuf);
  }
  //set TIA offset voltage in DAC units
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
  //testing only
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
  //set the voltage of the LT836X boost converter, smaller values give higher voltage, always set >250mV higher than output voltage
  else if (strcmp(tok, "dac") == 0 || strcmp(tok, "lt8362x") == 0 || strcmp(tok, "lt8361") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o > 1023 || o < 0)
      Serial.println("offset invalid (out of range [0 1023])");
    else
    {
      lt8361_voltage = o;
      //must write to A alias of the pin or its interpreted as digital
      analogWrite(A0, o);
    }
    // Always reply something:
    sprintf(msgbuf, "set dac A0 to %d dn (%d millivolts)\n", o, lt8361_voltage_millivolts(o));
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "lt8362x_voltage") == 0 || strcmp(tok, "lt8361_voltage") == 0 || strcmp(tok, "lt8362_voltage") == 0)
  {
    tok = strtok(NULL, " \n");
    long o = strtol(tok, NULL, 10);
    if (o >= 6 || o <= 65)
      Serial.println("offset invalid (out of range [6 65])");
    else
    {
      lt8361_voltage =  lt8361_dn_from_millivolts(o);
      //must write to A alias of the pin or its interpreted as digital
      //analogWrite(A0, lt8361_voltage);
    }
    // Always reply something:
    sprintf(msgbuf, "set dac A0 to %d dn (%d millivolts)\n", o, lt8361_voltage);
    Serial.write(msgbuf);
  }
  
  else if (strcmp(tok, "dac?") == 0 || strcmp(tok, "lt8362x_voltage?") == 0)
  {
    sprintf(msgbuf, "Arduino DAC setting LT836x to %d DNs\n", lt8361_voltage);
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
  else if (strcmp(tok, "volts?") == 0 || strcmp(tok, "gain_voltage?") == 0 || strcmp(tok, "voltage?") == 0)
  {
    sprintf(msgbuf, "%d\n", millivoltFromGain(gain));
    Serial.write(msgbuf);
  }
  // Change gain voltage input of milliVolts
  else if (strcmp(tok, "millivolts") == 0 || strcmp(tok, "gain_millivolts") == 0 || strcmp(tok, "voltage") == 0 || strcmp(tok, "volts") == 0 || strcmp(tok, "gain_voltage") == 0)
  { 
    long scaler = 1;
    //check units of command and convert to millivolts if needed
    if (strcmp(tok, "voltage") == 0 || strcmp(tok, "gain_voltage") == 0 || strcmp(tok, "volts") == 0)
    {
      sprintf(msgbuf, "In If because %s\n",tok);
      Serial.write(msgbuf);      
      scaler = 1000;
    }
    
    tok = strtok(NULL, " \n");

    long volts = strtol(tok, NULL, 10);
    volts = volts * scaler;


    uint32_t vmin = millivoltFromGain(0);    // minimum millivolt depends on calibration
    uint32_t vmax = millivoltFromGain(4095); // maximum millivolt depeneds on calibration
    uint16_t g = gainFromVoltage(volts); // Has input of millivolts
    if (volts > vmax || volts < vmin)
    {
      sprintf(msgbuf, "voltage invalid (%d out of range [%d %d] mV\n",volts, vmin, vmax);
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
    long tg42, tg50;
    if(1)
    {
      //calibration where it sets a gain and asks for a voltage

      setVoltageAndOffset(g42, offset);
      
      Serial.print("Set gain to ");
      Serial.print(g42);
      Serial.println(" DN, please enter the actual voltage in volts:"); 
      

      while(Serial.available()==0){}
      
      cmd = Serial.readStringUntil('\n');
      char *str = (char *)cmd.c_str();
      float f1 = strtof (str, NULL);

      Serial.println("Read: ");
      Serial.println(f1);

      //measure larger voltage
      setVoltageAndOffset(g50, offset);
      Serial.print("Set gain to ");
      Serial.print(g50);
      Serial.println(" DN, please enter the actual voltage in volts:"); 

      while(Serial.available()==0){}

      cmd = Serial.readStringUntil('\n');
      str = (char *)cmd.c_str();
      float f2 = strtof (str, NULL);

      float m = (f2-f1)/(g50-g42);
      float b = f1 - g42*m;


      Serial.print("Calculated m/b ");
      Serial.println(m, 5);
      Serial.println(b, 5);

      //TODO:  get rid of this legacy format and store just the equation of the line
      g42 = (42.0f-b)/m;
      g50 = (50.0f-b)/m;

      
    }
    else
    {
      // calibration gain@42v gain@50v
      tok = strtok(NULL, " \n");
      tg42 = strtol(tok, NULL, 10);
      tok = strtok(NULL, " \n");
      tg50 = strtol(tok, NULL, 10);
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
    }
    
    
    sprintf(msgbuf, "Calibrated 42V to %d, 50V to %d\n", g42, g50);
    Serial.write(msgbuf);
  }
  else if (strcmp(tok, "write_rom") == 0 || strcmp(tok,"write_offset") == 0)
  {
    writeEEPROM();
  }
  else if (strcmp(tok, "read_rom") == 0 || strcmp(tok,"read_offset") == 0)
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
  else if (strlen(tok) > 1)
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
    readEEPROM();
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