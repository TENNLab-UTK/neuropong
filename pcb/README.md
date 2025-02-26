# Atari CTRL PCB
This folder contains the KiCad source files along with some renders of what the PCB looks like

# Components
1. Digital Potentiometer - AD5242
    - Function: Emulate paddle controller analogue signal
    - https://www.digikey.com/en/products/detail/analog-devices-inc/AD5242BRUZ1M/997427?s=N4IgTCBcDaIKwAY4FoCCAROYAsYBCASgKoBaAjALLIBy6IAugL5A
2. Darlington Transistor Pair - ULN2003A
    - Function: Emulate button presses from joystick controller (negative logic)
    - Note: When button is pressed, the input pin (UP/DOWN/LEFT/RIGHT/FIRE) is shorted from floating voltage to ground (0 V)
    - https://www.digikey.com/en/products/detail/texas-instruments/ULN2003ADR/277011

## Case Incompatability Disclosure 
**NOTE**: The PCB Case Lid is only compatible with 1.25A version of the case. This is due to the connector size changes from 2.00mm connector (v1.27) to a 2.54mm connector (1.37A). 

## Pinout Disclosure
**NOTE**: The PCB design does not utilize the 5V rail from the Atari to avoid voltage mismatches on the digital potentiometer output.
