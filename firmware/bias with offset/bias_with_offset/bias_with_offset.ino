#include <Wire.h>
#include <Adafruit_MCP4728.h>
#include <FlashAsEEPROM.h>

// Reference Voltage definitions
#define DEVICE_NAME "SiPM 1.2"
char NAME[] = DEVICE_NAME;

uint8_t config;
uint16_t gain = 100;
uint16_t offset = 250;
uint16_t g42=0, g50=0;
bool on = LOW;
uint8_t devAddr = 0x60;
int eeAddress = 0;
String cmd;

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

  //readEEPROM();
}

void setVoltageSIPM( uint16_t output)
{
  //note:  reference voltage is 2.048, but maximum gain is at 1.0v, so limit range
  if(output> 2000)
  {
    output = 2000;
  }
  
  //note:  apd voltage is Vout*(R1/R2+1) where R1=1M and R2=15k
  //for some reason the voltage seems to be a bit lower
   mcp.setChannelValue(MCP4728_CHANNEL_A, output, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_1X);
}

void setVoltageOffset( uint16_t output)
{
  
   mcp.setChannelValue(MCP4728_CHANNEL_B, output, MCP4728_VREF_INTERNAL,
                      MCP4728_GAIN_1X);
}

uint16_t gainFromVoltage(float voltage)
{
  if(g42<100 || g50<100)
    return 0;
  //linear interpolation using 42 and 50V 
  return (uint16_t)((g42/8.0f)*(50.0f-voltage) + (g50/8.0f)*(voltage-42.0f));
  
}

void writeEEPROM(uint16_t offset)
{
  EEPROM.update(eeAddress, offset);
  EEPROM.update(eeAddress+1, offset>>8);

  //save calibration
  EEPROM.update(eeAddress+2, g42);
  EEPROM.update(eeAddress+3, g42>>8);
  EEPROM.update(eeAddress+4, g50);
  EEPROM.update(eeAddress+5, g50>>8);
    
  
  EEPROM.commit();
}

void readEEPROM()
{
  //check if a sane value is present and if so use that as the offset
  uint16_t temp = (EEPROM.read(eeAddress+1) << 8 ) | (EEPROM.read(eeAddress) & 0xff);
  
  if(temp > 50 && temp < 1000){
    offset = temp;
    setVoltageOffset(offset);
  }  
  else
  {
    char buf[30];
    sprintf(buf, "Invalid EEPROM value: '%d'\n", temp);
    Serial.print(buf);
    return;
  }

  temp = (EEPROM.read(eeAddress+3) << 8 ) | (EEPROM.read(eeAddress+2) & 0xff);
  if(temp > 20 && temp < 2000){
    g42 = temp;  
  }
  
  temp = (EEPROM.read(eeAddress+5) << 8 ) | (EEPROM.read(eeAddress+4) & 0xff);
  if(temp > 20 && temp < 2000){
    g50 = temp;  
  }

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
    if (on) Serial.write("on\n");
    else Serial.write("off\n");
  }
  else if (strcmp(tok, "gain") == 0) {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4095 || g < 0) return;
    gain = (uint16_t)g;
    setVoltageSIPM(gain);
  }
  else if (strcmp(tok, "gain?") == 0) {
    char buf[30];
    sprintf(buf, "%d\n", gain);
    Serial.write(buf);
  }
  else if (strcmp(tok, "offset") == 0) {
    tok = strtok(NULL, " \n");
    long g = strtol(tok, NULL, 10);
    if (g > 4095 || g < 0) return;
    offset = (uint16_t)g;
    setVoltageOffset(offset);
  }    
  else if (strcmp(tok, "offset?") == 0) {
    char buf[30];
    sprintf(buf, "%d\n", offset);
    Serial.write(buf);
  }
  else if (strcmp(tok, "voltage") == 0 || strcmp(tok, "volts") == 0) {
    tok = strtok(NULL, " \n");
    float volt = strtof(tok, NULL);
  
    if (volt < 0 || volt > 60) return;
    gain = gainFromVoltage(volt);
    //sprintf(buf, "got2: %d\n", gain);
    //Serial.write(buf);
    setVoltageSIPM(gain);
  }  
  else if (strcmp(tok, "calibration") == 0) {  //calibration gain@42v gain@50v
    tok = strtok(NULL, " \n");
    g42 = strtol(tok, NULL, 10);
    if (g42 > 4095 || g42 < 0) 
    {
      g42=0;
      g50=0;
      return;
    }
    tok = strtok(NULL, " \n");
    g50 = strtol(tok, NULL, 10);
    if (g50 > 4095 || g50 < 0)
    {
      g42=0;
      g50=0;
      return; 
    }  
    char buf[30];
    sprintf(buf, "Calibrated 42V to %d, 50V to %d\n", g42, g50);    
    Serial.write(buf);
    
  }  
  else if (strcmp(tok, "write_offset") == 0) {
    writeEEPROM(offset);
  }
  else if (strcmp(tok, "read_offset") == 0) {
     Serial.write("reading offset\n");
    readEEPROM();
  }  
  else if (strcmp(tok, "name?") == 0) {
    Serial.write(NAME);
  }
  else if(strlen(tok)>2)
  {
    char buf[30];
    sprintf(buf, "Unknown command: '%s'\n", tok);
    Serial.write(buf);
  }
}

int loopcount=0;
void loop() 
{
  if(Serial.available()){
    cmd = Serial.readStringUntil('\n');
    char *str = (char*)cmd.c_str();
    parse_command(str);
  }

  delay(50);
  digitalWrite(13, on); 

  //wait 3 seconds and then apply settings from ROM 
  if(loopcount > 101)
  {
   //do nothing, settings have already been loaded
  }
  else
  {
    if(loopcount == 100)
    {
      readEEPROM();
    }
    loopcount++;

  }

}
