# Hallowing laughing eye

This is a quick project to make a laughing eye for a Halloween pumpkin using the Hallowing M4 board.

The configuration for the looks of the eye and the laughter sound files are in the config folder, they should be
copied on the board when it is not in bootloader mode. See [Adafruit guide](https://learn.adafruit.com/adafruit-hallowing-m4)
for more details on the configuration of the looks of the eye.

The code in the `code` folder needs to be copied in as one of the user_* files in the [c++ project](https://github.com/adafruit/Adafruit_Learning_System_Guides/tree/main/M4_Eyes)
while making sure no others are enabled. This [blog post](https://rebeccamdeprey.com/blog/programming-the-hallowing-m4)
was very helpful in getting started with that and setting up Arduino IDE.

The code makes the laughter sound every minute, cycling through 3 different laughters. While the laugh is
playing, the neopixels on board flash red. Other times they cycle through flame like hues of yellow, orange and red.

I used a modified [3D-printed case](https://www.thingiverse.com/thing:6809956) for this project.