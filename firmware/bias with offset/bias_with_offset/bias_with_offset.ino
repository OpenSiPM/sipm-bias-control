#include <Wire.h>
#include <Adafruit_MCP4728.h>
#include <FlashAsEEPROM.h>

// Reference Voltage definitions
#define DEVICE_NAME "SiPM 1.1"
char NAME[] = DEVICE_NAME;

uint8_t config;
uint16_t gain = 100;
uint16_t offset = 250;
bool on = LOW;
uint8_t devAddr = 0x60;
int eeAddress = 0;

Adafruit_MCP4728 mcp;

void setup() 
{
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  digitalWrite(13, on);
  Wire.begin();

  delay(500);

    // Try to initialize!
  if (!mcp.begin()) {
    Serial.println("Failed to find MCP4728 chip");

  }
  else
  {
    Serial.print("Found MCP4728 chip");
    mcp.setChannelValue(MCP4728_CHANNEL_B, 2048);
  }
}

void setVoltageSIPM( uint16_t output)
{
  //note:  reference voltage is 2.048, but maximum gain is at 1.0v, so limit range
  if(output> 2000)
  {
    output = 2000;
  }
  
  //note:  apd voltage is Vout*(R1/R2+1) where R1=1M and R2=20.5k
   mcp.setChannelValue(MCP4728_CHANNEL_A, output, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_1X);
}

void setVoltageOffset( uint16_t output)
{
  
   mcp.setChannelValue(MCP4728_CHANNEL_B, output, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_1X);
}

void writeEEPROM(uint16_t offset)
{
  EEPROM.update(eeAddress, offset);
  EEPROM.commit();
}

void readEEPROM()
{
  offset = EEPROM.read(eeAddress);
}

void parse_command(char *str) {
 
  
  char *tok;
  tok = strtok(str, " \n");

  if (strcmp(tok, "on") == 0) {
    on = true;
  }
  else if (strcmp(tok, "off") == 0) {
    on = false;
  }
  else if (strcmp(tok, "on?") == 0) {
    if (on) Serial.print("on\n");
    else Serial.print("off\n");
  }
  else if (strcmp(tok, "gain") == 0) {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4095 || g < 0) return;
    gain = (uint16_t)g;
  }
  else if (strcmp(tok, "gain?") == 0) {
    char buf[30];
    sprintf(buf, "%d\n", gain);
    Serial.print(buf);
  }
  else if (strcmp(tok, "offset") == 0) {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4095 || g < 0) return;
    offset = (uint16_t)g;
  }    
  else if (strcmp(tok, "offset?") == 0) {
    char buf[30];
    sprintf(buf, "%d\n", offset);
    Serial.print(buf);
  }
  else if (strcmp(tok, "write offset") == 0) {
    writeEEPROM(offset);
  }
  else if (strcmp(tok, "read offset") == 0) {
     Serial.print("reading offset");
    readEEPROM();
  }  
  else if (strcmp(tok, "name?") == 0) {
    Serial.print(NAME);
  }
  else if(strlen(tok)>2)
  {
    char buf[30];
    sprintf(buf, "Unknown command: '%s'\n", tok);
    Serial.print(buf);
  }
}

void loop() 
{
  String cmd = Serial.readString();
  char *str = (char*)cmd.c_str();

  parse_command(str);

  delay(100);
  digitalWrite(13, on);
  
  setVoltageSIPM(gain);
  setVoltageOffset(offset);



}
