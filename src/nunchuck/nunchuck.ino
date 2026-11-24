#include "USB.h"
#include "USBHIDMouse.h"
#include <Adafruit_NeoPixel.h>

// helper functions
#define sgn(x) ((x) < 0 ? -1 : ((x) > 0 ? 1 : 0))
#define square(x) (x * x)

// declare mouse
USBHIDMouse Mouse;

// define NeoPixel
#define RGB_PIN 48
#define NUM_LEDS 1

const int brightness = 150;

Adafruit_NeoPixel pixel(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Define pins
#define LEFT_BUTTON 1
#define RIGHT_BUTTON 2
#define MIDDLE_BUTTON 6

#define XAXIS_PIN 4
#define YAXIS_PIN 5

#define BOOT_BUTTON 0

// define movement and rates
int xAxisCenter = 113;
int yAxisCenter = 114;

int rangeIndex = 2;
int ranges[] = {5, 10, 15, 20, 30};
const int rangeLen = 5;

class PID {
  private:
    float x = 0;
    float last = NAN;
    float integ = 0;

  public:
    float kp;
    float ki;
    float kd;

    PID(float kp = 0.003, float ki = 0.0, float kd = 0.3) {
      this->kp = kp;
      this->ki = ki;
      this->kd = kd;
    }

    void reset() {
      this->x = 0;
      this->last = NAN;
      this->integ = 0;
    }

    float step(float targ, float dt) {
      float error = targ - this->x;

      float deriv = 0;
      if (!isnan(this->last)) {
        deriv = (error - this->last) / dt;
      }
      this->last = error;

      this->integ += error * dt;

      float accel = error * this->kp + this->integ * this->ki + deriv * this->kd;
      this->x += accel * dt;
      return this->x;
    }
};
PID pid;

float deadZone = 0.55;
int responseDelay = 10;

// button states
struct Button {
  unsigned long lastDebounce = 0;
  int state = 0;
  int lastState = 0;
};

struct Buttons {
  Button left;
  Button right;
  Button middle;
  Button boot;
};

Buttons buttons;

// debouncing delay
const unsigned long debounceDelay = 50;

void setup() {
  // Setup serial
  Serial.begin(115200);

  // Setup pixel
  pixel.begin();
  pixel.setBrightness(brightness);
  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();

  // Set pin modes
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON, INPUT_PULLUP);
  
  pinMode(XAXIS_PIN, INPUT);
  pinMode(YAXIS_PIN, INPUT);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // Setup mouse
  Mouse.begin();
  USB.begin();

  // Reset debounces
  unsigned long initDebounce = millis() + debounceDelay;
  buttons.left.lastDebounce = initDebounce;
  buttons.right.lastDebounce = initDebounce;
  buttons.middle.lastDebounce = initDebounce;
  buttons.boot.lastDebounce = initDebounce;

  Serial.println("Nunchuck V3 - Ready");
}

void loop() {
  // Check buttons
  checkButton(LEFT_BUTTON, &buttons.left.state, &buttons.left.lastState, &buttons.left.lastDebounce);
  checkButton(RIGHT_BUTTON, &buttons.right.state, &buttons.right.lastState, &buttons.right.lastDebounce);
  checkButton(MIDDLE_BUTTON, &buttons.middle.state, &buttons.middle.lastState, &buttons.middle.lastDebounce);

  // Get access values
  float xAxis = readAxis(XAXIS_PIN, xAxisCenter);
  float yAxis = readAxis(YAXIS_PIN, yAxisCenter);

  // adjust mouse movements
  adjustMovements(&xAxis, &yAxis);

  // Move mouse
  if ((xAxis != 0) || (yAxis != 0)) {
    // move mouse
    Mouse.move(xAxis, yAxis);
  }

  // reset pid
  else {
    pid.reset();
  }

  // Do buttons
  doButton(buttons.left.state, MOUSE_LEFT);
  doButton(buttons.right.state, MOUSE_RIGHT);
  doButton(buttons.middle.state, MOUSE_MIDDLE);

  doBootButton(BOOT_BUTTON, &buttons.boot.state, &buttons.boot.lastState, &buttons.boot.lastDebounce);

  // Update pixel
  if (buttons.boot.state == LOW) {
    pixel.setPixelColor(0, pixel.Color(brightness, brightness, brightness));
  } else {
    pixel.setPixelColor(0, pixel.Color(brightness * (1 - buttons.right.state), brightness * (1 - buttons.left.state), brightness * (1 - buttons.middle.state)));
  }
  pixel.show();

  // Delay loop
  delay(responseDelay);
}

float readAxis(int axisPin, int center) {
  // Read and calculate mouse pixel displacement
  int range = ranges[rangeIndex];

  double raw = map(analogRead(axisPin), 0, 4095, 0, 255) - center; // center
  raw = 127 * (raw > 0 ? raw / (255 - center) : raw / center); // adjust end points
  return max(-127.0, min(127.0, raw)) * (range + deadZone) / 127; // clamp and scale
}

void adjustMovements(float *xAxis, float *yAxis) {
  // normalize mouse axis movements
  float range = ranges[rangeIndex];

  // calcualte vector length
  float len = sqrt(square(*xAxis) + square(*yAxis));

  // deadzone
  if (len < deadZone) {
    *xAxis = 0;
    *yAxis = 0;
  }

  else {
    // apply movement curve
    float mag = range * square(max((float) 0, len - deadZone) / range);

    // trim if magnitude is too long
    mag = min(range, mag);

    // Plot it out first
    //Serial.print("T:");
    //Serial.print(mag);

    // pid control
    mag = pid.step(mag, responseDelay);
    //Serial.print(",X:");
    //Serial.println(mag);

    // calculate normalized axis values
    float x = *xAxis / len * mag;
    float y = *yAxis / len * mag;

    // assign to axis values
    *xAxis = x;
    *yAxis = y;
  }
}

int checkButton(int buttonPin, int *state, int *lastState, unsigned long *last) {
  // Check button and adjust for debounce
  int read = digitalRead(buttonPin);

  if (read != *lastState) {
    *last = millis();
  }

  if ((millis() - *last) > debounceDelay) {
    if (read != *state) {
      *state = read;
    }
  }

  *lastState = read;

  return *state;
}

void doButton(int state, int button) {
  // Do mouse button press
  if (state == LOW) {
    if (!Mouse.isPressed(button)) {
      Mouse.press(button);
    }
  }
  else {
    if (Mouse.isPressed(button)) {
      Mouse.release(button);
    }
  }
}

int doBootButton(int buttonPin, int *state, int *lastState, unsigned long *last) {
  // Do boot button debounce and actions
  int read = digitalRead(buttonPin);

  if (read != *lastState) {
    *last = millis();
  }

  if ((millis() - *last) > debounceDelay) {
    if (read != *state) {
      *state = read;

      // cycle through mouse pixel steps (aka ranges)
      if (read == HIGH) {
        if (rangeIndex + 1 < rangeLen) {
          rangeIndex++;
        } else {
          rangeIndex = 0;
        }

        Serial.print("Updated Range: ranges[");
        Serial.print(rangeIndex);
        Serial.print("] -> ");
        Serial.println(ranges[rangeIndex]);
      }
    }
  }

  *lastState = read;

  return *state;
}