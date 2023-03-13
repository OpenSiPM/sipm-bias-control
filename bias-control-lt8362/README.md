# Introduction

![image](https://user-images.githubusercontent.com/16110774/224592440-e20e4623-2d26-419e-829d-9275bcd94b29.png)

This is a new version of the Bias Generator that has an additional customizable current limiting feature, modified components and been designed for easier construction. It also has much higher output current and can be used to drive larger SiPMs or arrays of multiple SiPMs.  


# System - Voltage Drop off at Various Currrent Levels w/o Peak Regulator
The system has been tested to be limited to approximately 33mA at ~50V see provided graphs below to see curve of Voltage vs Current.
<img width="422" alt="image" src="https://user-images.githubusercontent.com/21182901/167532900-df6c0ab5-f566-453c-a1a4-490e9dddc416.png">


# Current Limiting Feature
The current limit can be changed by adjusting Resisitor 13 however the max possible current limit is set by the general voltage drop off of board without the current limiting feature. The cutoff current can be set by using the formula below. See examples of Voltage vs Current curves for 5Ω and 10Ω resistors below.
Formula: 

<img width="361" alt="image" src="https://user-images.githubusercontent.com/21182901/167533017-04e06f4c-6364-4a5a-95fe-0270fc58a49f.png">


# Key Component Changes
LT8362 boost converter enables much higher output current.  
RT9072A High voltage LDO for filtering provides much better isolation of the boost converter from the load.
USB-B micro instead of C to simplify assembly.

# Firmware changes
Use the bias_with_offset_LT836X firmware. The firmware now sets both the boost converter voltage (by default to about 52V) and controls the output voltage using the LDO.  Both are under software control, so you can change the indepedently.  Use the "dac" command to set the boost converter voltage.   

# Ease of Construction and Use
All components have been changed to enable hand soldering of boards as well as increased spacing between critical functions. 
1. All capacitors and resistors changed from 0402 to 0603
2. All chips have leg pins for easier soddering
3. Testpoints for Ground and High Voltage

