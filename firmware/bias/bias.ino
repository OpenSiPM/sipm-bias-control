#include <Wire.h>
#include <MCP4726.h>
 
#define DOUT  0   //M0 9 //m0tr - 0
#define CLK   2   //M0 10//m0tr - 2

#define DEVICE_NAME "SiPM"
char NAME[] = DEVICE_NAME;

uint16_t gain = 0;
bool on = LOW;

void setup() 
{
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  digitalWrite(13, on);
  Wire.begin();
}

void setVoltage( uint16_t output)
{
  Wire.beginTransmission(0x60);
  Wire.write((uint8_t) ((output >> 8) & 0x0F));   // MSB: (D11, D10, D9, D8) 
  Wire.write((uint8_t) (output));  // LSB: (D7, D6, D5, D4, D3, D2, D1, D0)
  Wire.endTransmission();
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
  else if (strcmp(tok, "name?") == 0) {
    Serial.print(NAME);
  }
}

void loop() 
{
  String cmd = Serial.readString();
  char *str = (char*)cmd.c_str();

  parse_command(str);

  digitalWrite(13, on);
  setVoltage(gain);
}
