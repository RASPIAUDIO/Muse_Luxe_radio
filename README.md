# The full tutorial is here:
https://forum.raspiaudio.com/t/muse-luxe-internet-radio/301

# museLuxe_radio
Here is a radio app using a Muse Luxe board

#### It uses 3 buttons:
   - VM (gpio 32)
        - short press => volume -
        - long press => previous radio
   - VP (gpio 19)
        - short press => volume +
        - long press => next radio
   - MU (gpio 12)
        - short press => mute/unmute
        - very long press => stop (deep sleep) / restart

#### And a little screen (64x128 SH1106) (sda gpio 18, scl gpio 15) using the grove connector on the back
   	(optional)


## How to customize your app ?

There  are two ways do to this:

		1. With Arduino
				- You have to edit two files :
						. data/nameS  => the names of your prefered radios
						. data/linkS  => the links to these radios
				- and then copy them to the flash memory (=> Tools =>  ESP32 Sketch Data Upload)

		2. Using the "parameter mode"  
				- to switch to this mode, keep the MU button pressed while the app restarts
                  (The app automatically switches to this mode if it cannot connect to WiFi)
				- then with your smartphone connect to WiFi spot "Muse" (password : "musemuse")
				- from there you will be able :
						. to set the credentials of your wifi
						. to add, modify, delete your radios

## Before building it using Arduino

   You have to add  the Muse specific library (muse_lib) to Arduino libraries, copy the mus_lib file in you library directory, then restart the Arduino application?

   For example using these bash commands :

             > cd ..../museProto_radio
             > cp -r muse_lib ..../Arduino/libraries

## Target option in Tools =>

	Board: ESP32 Wrove module
	Upload speed : 921600
	Flash frequency: 80mHz
	Flash mode : QIO
	Partition Scheme : Huge App






 ....                    
