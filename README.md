wii-esc
========

## Branches
master - Clone of google code wii-esc. Don't expect to be up-to date

two_phase_ptu - extra code to drive two phase camera pan tilt units with a standard ESC

## General Notes
* Have tested with hobbyking F-40A, usually driven with at least a 24V supply.
* Make with "make -f makefile.ptu"
* "make upload" target is for a buspirate, customise to suit your chosen programmer.
* PWM values less than 1450 are one direction, greater than 1550 other direction, in between is still.
* Use the centre output for common rail on 2-phase motors (this is true for an F-40A, don't blame me if your esc smokes)
