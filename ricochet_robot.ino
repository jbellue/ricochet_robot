#include <Adafruit_NeoPixel.h>

/*
Ricochet Robots made awesome

This is a conversion to the digital age of the best game ever, Ricochet Robots.
There are LED_NUMBER leds (16 would be as close to the game as possible).
On the press of a button, one led will be lit in a random color (red, yellow,
green or blue). There is a 1/RANDOMNESS_NUMBER chance the led will instead be
changing colors rapidly (this is to replace the multicolored vortex tile).

That led will then pulse, until a player presses the button, triggering the
timer (the whole strip should blinkto show the timer started).

The timer then runs for TIMER_LENGTH, and the strip will blink once more
TIMER_WARN ms before the end. After that, the strip will blink three times,
to show the timer's ended.

The player shows their route (the LED has stopped pulsing, to show you shouldn't
be looking for a route anymore, in case you'd missed that).

Then, on the next button press, a new LED is selected, and the whole process
starts again.

At any time, a long press on the button (longer than HOLD_TIME seconds) will
reset the loop, and select a new led. This is to avoid having to wait for the
whole timer when the route is obvious.

TODO
1- become rich with this code
*/

/* user defined vairables */
#define RANDOMNESS_LEVEL     16
#define TIMER_LENGTH         10000
// warn before the end of the timer (in ms)
#define TIMER_WARN           5000

// how long you need to hold to reset the loop
#define HOLD_TIME            1000

// ~3/4
#define STRIP_MIN_BRIGHTNESS 50
#define PULSE_DELAY          2

// how fast to run the space thingie
#define SPACE_DELAY          100

// number of steps to take during space
#define SPACE_STEPS_NUMBER   10

// how long each blink is
#define BLINK_LENGTH         100
/* ****************************************** */

/* hardware variables, don't touch unless you know what you're doing */
#define LED_NUMBER       5
#define BTN_PIN          0
#define STRIP_PIN        1
#define LED_PIN          2
#define DEBOUNCE_DELAY   20
/* ****************************************** */

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_NUMBER, STRIP_PIN, NEO_GRB + NEO_KHZ800);

/*
  State machine explanation:
STATE_STARTING      -> starting state, initialises stuff and goes to STATE_PICKING_LED
STATE_PICKING_LED   -> picks a led and lights it, goes to STATE_LOOKING
STATE_LOOKING     -> waits for a click, just vortexing.
              On click, goes to STATE_RUNNING_TIMER
STATE_RUNNING_TIMER   -> runs the timer, blinks once TIMER_WARN before then end of the timer
                  three times at the end of it, and goes to STATE_SHOWING_SOLUTION
STATE_SHOWING_SOLUTION  -> the players shows the solution, nothing happens.
              On click, goes to STATE_PICKING_LED
*/

// button click vars
unsigned long lastDebounceTime = 0;   // last time the output pin was toggled
bool buttonState;
bool lastButtonState = LOW;
unsigned long btnDnTime;
unsigned long btnUpTime;
bool ignoreUp = false;

enum {
    STATE_STARTING,
    STATE_PICKING_LED,
    STATE_LOOKING,
    STATE_RUNNING_TIMER,
    STATE_SHOWING_SOLUTION
} state;

// vortex colours vars
byte r = 0x00;
byte g = 0x00;
byte b = 0xFF;
byte rShift = 1;
byte gShift = 0;
byte bShift = 0;
bool vortexLed = false;

// pulse things
byte isGettingBrighter  = 1; // +1 or -1, to increase/decrease brightness
unsigned long pulseTime = 0;

//space vars
unsigned long spaceTime = 0;
byte spaceIndex         = 0;   // space thing step number
byte formerSpaceChip;
byte thisSpaceChip;

// blinkage thingies
// this is a terrible pain. I really hope I never have to look again at that
// code. Yuk.
byte blinkStep = 0;

//timer stuff
unsigned long lastTimerTime = 0;

const uint32_t colours[4] =
{
  0xFF0000, //red
  0xFFFF00, //yellow
  0x00FF00, //green
  0x0000FF  //blue
};
byte colourIndex = 0;
byte thisChip    = 0;

void setup()
{
  pinMode(BTN_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(BTN_PIN, HIGH);
  strip.begin();
  strip.show();
  state = STATE_STARTING;
}

void loop ()
{
  bool reading = digitalRead(BTN_PIN);

  // Test for button pressed and store the down time
  if (reading == LOW && lastButtonState == HIGH && (millis() - btnUpTime) > DEBOUNCE_DELAY)
  {
    btnDnTime = millis();
  }
  // Test for button release and store the up time
  if (reading == HIGH && lastButtonState == LOW && (millis() - btnDnTime) > DEBOUNCE_DELAY)
  {
    if (!ignoreUp)
    {
      if (state == STATE_STARTING)
      {
        state = STATE_PICKING_LED;
        randomSeed(millis());
      }
      else if (state == STATE_LOOKING)   // players are looking for a route
      {
        state = STATE_RUNNING_TIMER;
        lastTimerTime = millis();
      }
      else if (state == STATE_SHOWING_SOLUTION)   // player is showing the route
      {
        strip.setPixelColor(thisChip, 0);
        strip.show();
        state = STATE_PICKING_LED;
        initialise();
        return;
      }
    }
    else
    {
      ignoreUp = false;
    }
    btnUpTime = millis();
  }

  // Test for button held down for longer than the hold time
  if (reading == LOW && (millis() - btnDnTime) > HOLD_TIME)
  {
    initialise();
    state = STATE_PICKING_LED;
    ignoreUp = true;
    btnDnTime = millis();
  }

  switch (state)
  {
    case STATE_STARTING: //starting state
      initialise();
      break;
    case STATE_PICKING_LED: //pick and light led
      if (!spaceThing())
      {
        pickLED();
        pickColour();
        if (!runVortexIfNeeded())
        {
          strip.setPixelColor(thisChip, colours[colourIndex]);
          strip.show();
        }
        state = STATE_LOOKING;
      }
      break;
    case STATE_LOOKING: //players are looking for a route
      runVortexIfNeeded();
      pulse();
      break;
    case STATE_RUNNING_TIMER: //timer is running
      runVortexIfNeeded();
      pulse();
      timerBlink();
      break;
    case STATE_SHOWING_SOLUTION:
      runVortexIfNeeded();
      strip.setBrightness(255);
      isGettingBrighter = 1;
      strip.show();
      break;
    default:
      break;
  }
  lastButtonState = reading;
}

void initialise()
{
  strip.setPixelColor(thisChip, 0);
  digitalWrite(LED_PIN, LOW);
  resetBrightness();
  blinkStep = 0;
  spaceIndex = 0;
  strip.show();
}

void pickLED()    // haha, pickled
{
  byte formerChip = thisChip;
  do
  {
    thisChip = random (0, LED_NUMBER);
  } while (formerChip == thisChip);
}

void pickColour()
{
  if (random(0, RANDOMNESS_LEVEL) == 0)
  {
    vortexLed = true;
  }
  else
  {
    vortexLed = false;
    colourIndex = random (0, 4);
  }
}

void setShifts(int r1, int g1, int b1)
{
  rShift = r1;
  gShift = g1;
  bShift = b1;
}

bool runVortexIfNeeded()
{
  if (vortexLed)
  {
    if (r == 0)
    {
      if (g == 255)
      {
        if (b == 0)
          setShifts(0, 0, 1);  // r==0   && g==255 && b==0
        else if (b == 255)
          setShifts(0, -1, 0); // r==0   && g==255 && b==255
      }
      else if (g == 0 && b == 255)
        setShifts(1, 0, 0);      // r==0   && g==0   && b==255
    }
    else if (r == 255)
    {
      if (g == 0)
      {
        if (b == 0)
          setShifts(0, 1, 0);  // r==255 && g==0   && b==0
        else if (b == 255)
          setShifts(0, 0, -1); // r==255 && g==0   && b==255
      }
      else if (g == 255 && b == 0)
        setShifts(-1, 0, 0);     // r==255 && g==255 && b==0
    }
  
    r += rShift;
    g += gShift;
    b += bShift;
    strip.setPixelColor(thisChip, r, g, b);
    strip.show();
    return true;
  }
  else
  {
    return false;
  }
}

void pulse()
{
  unsigned long currentPulseTime = millis();
  if (currentPulseTime - pulseTime > PULSE_DELAY)
  {
    pulseTime = currentPulseTime;
    byte stripBrightness = strip.getBrightness();
    if (stripBrightness >= 255 || stripBrightness <= STRIP_MIN_BRIGHTNESS)
    {
      isGettingBrighter = -isGettingBrighter;
    }

    strip.setBrightness(stripBrightness + isGettingBrighter);
    strip.show();
  }
}

// wow that's ugly... Meh, it works.
void timerBlink()
{
  unsigned long currentMillis = millis();
  unsigned long timerEnd = lastTimerTime + TIMER_LENGTH;

  if (blinkStep == 9 && currentMillis >= timerEnd)
  {
    // Ending the third ending blink
    switchStripOff();
    state = STATE_SHOWING_SOLUTION;
    blinkStep = 0;
  }
  else if (blinkStep == 8 && currentMillis >= timerEnd - BLINK_LENGTH)
  {
    // Starting the third ending blink
    switchStripOn();
    blinkStep++;
  }
  else if (blinkStep == 7 && currentMillis  >= timerEnd - 2 * BLINK_LENGTH)
  {
    // Ending the second ending blink
    switchStripOff();
    blinkStep++;
  }
  else if (blinkStep == 6 && currentMillis >= timerEnd - 3 * BLINK_LENGTH)
  {
    // Starting the second ending blink
    switchStripOn();
    blinkStep++;
  }
  else if (blinkStep == 5 && currentMillis  >= timerEnd - 4 * BLINK_LENGTH)
  {
    // Ending the first ending blink
    switchStripOff();
    blinkStep++;
  }
  else if (blinkStep == 4 && currentMillis >= timerEnd - 5 * BLINK_LENGTH)
  {
    // Starting the first ending blink
    switchStripOn();
    blinkStep++;
  }
  else if (blinkStep == 3 && currentMillis >=  timerEnd - TIMER_WARN + BLINK_LENGTH)
  {
    // Ending the warning blink
    switchStripOff();
    blinkStep++;
  }
  else if (blinkStep == 2 && currentMillis >= timerEnd - TIMER_WARN)
  {
    // Starting the warning blink
    switchStripOn();
    blinkStep++;
  }
  else if (blinkStep == 1 && currentMillis  >= lastTimerTime + BLINK_LENGTH)
  {
    // Ending the "solution found!" blink
    switchStripOff();
    blinkStep++;
  }
  else if (blinkStep == 0 && currentMillis >= lastTimerTime)
  {
    // Starting the "solution found!" blink
    switchStripOn();
    blinkStep++;
  }
}

void switchStripOff()
{
  for (byte i = 0 ; i < LED_NUMBER ; i++)
  {
    if (i != thisChip)
    {
      strip.setPixelColor(i, 0);
    }
  }
  strip.show();
}

void switchStripOn()
{
  resetBrightness();
  for (byte i = 0 ; i < LED_NUMBER ; i++)
  {
    if (i != thisChip)
    {
      if (vortexLed)
      {
        strip.setPixelColor(i, colours[random (0, 4)]);
      }
      else
      {
        strip.setPixelColor(i, colours[colourIndex]);
      }
    }
  }
  strip.show();
}

bool spaceThing()   // SPAAAAAAACE
{
  if (spaceIndex < SPACE_STEPS_NUMBER)
  {
    unsigned long currentSpaceTime = millis();
    if (currentSpaceTime - spaceTime > SPACE_DELAY)
    {
      spaceTime = currentSpaceTime;
      do
      {
        thisSpaceChip = random (0, LED_NUMBER);
      } while (formerSpaceChip == thisSpaceChip);

      strip.setPixelColor(thisSpaceChip, colours[random (0, 4)]);
      strip.setPixelColor(formerSpaceChip, 0);
      strip.show();
      spaceIndex++;
      formerSpaceChip = thisSpaceChip;
    }
    return true;
  }
  else
  {
    strip.setPixelColor(formerSpaceChip, 0);
    strip.setPixelColor(thisSpaceChip, 0);
    strip.show();
    spaceIndex = 0;
    return false;
  }
}

void resetBrightness()
{
  strip.setBrightness(255);
  isGettingBrighter = 1;
}
