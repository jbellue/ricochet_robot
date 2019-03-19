# Ricochet Robots
This is a conversion to the digital age of the best game ever, Ricochet Robots.

I had that running on an Adafruit trinket, but all arduino-compatible boards should be compatible.
A strip of LED_NUMBER  (16 would be as close to the game as possible) neopixels is connected to STRIP_PIN (here, pin 1).

On the press of a button (here, pin 2), one led will be lit in a random color (red, yellow, green or blue).
There is a 1/RANDOMNESS_NUMBER chance the led will instead be changing colors rapidly (this is to replace the multicolored vortex tile).
That led will then pulse, until a player presses the button, triggering the timer (the whole strip should blink to show the timer has started).
The timer then runs for TIMER_LENGTH, and the strip will blink once more TIMER_WARN ms before the end. After that, the strip will blink three times, to show the timer's ended.
The player shows their route (the LED has stopped pulsing, to show you shouldn't be looking for a route anymore, in case you'd missed that).
Then, on the next button press, a new LED is selected, and the whole process starts again.
At any time, a long press on the button (longer than HOLD_TIME seconds) will reset the loop, and select a new led. This is to avoid having to wait for the whole timer when the route is obvious.

## TODO
- probably refactor the whole thing
- implement it on a 16 by 16 grid to allow changing the board (that would require a whole other layer of UI):
  - rotary encoder to define X and Y of new LED
  - LCD screen to show what's going on
  - possibility to save / load configurations
