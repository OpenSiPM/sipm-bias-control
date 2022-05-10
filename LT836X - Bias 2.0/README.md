# Introduction

This is a new version of the Bias Generator that has an additional customizable current limiting feature, modified components and been designed for easier construction.


# System - Voltage Drop off at Various Currrent Levels w/o Peak Regulator
The system has been tested to be limited to approximately 33mA at ~50V see provided graphs below to see curve of Voltage vs Current.
<img width="422" alt="image" src="https://user-images.githubusercontent.com/21182901/167532900-df6c0ab5-f566-453c-a1a4-490e9dddc416.png">


# Current Limiting Feature
The current limit can be changed by adjusting Resisitor 13 however the max possible current limit is set by the general voltage drop off of board without the current limiting feature. The cutoff current can be set by using the formula below. See examples of Voltage vs Current curves for 5Ω and 10Ω resistors below.
Formula: 
<img width="361" alt="image" src="https://user-images.githubusercontent.com/21182901/167533017-04e06f4c-6364-4a5a-95fe-0270fc58a49f.png">


# Key Component Changes


# Ease of Construction and Use
All components have been changed to enable hand soldering of boards as well as increased spacing between critical functions. 
1. All capacitors and resistors changed from 0402 to 0603
2. All chips have leg pins for easier soddering
3. Testpoints for Ground and High Voltage

