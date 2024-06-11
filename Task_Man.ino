#include <Adafruit_SSD1306.h>

// Pin numbers
#define BUTTON 1
#define SDA 8
#define SCL 9

// Timing variables
#define BOUNCE 20
#define HOLD 750
#define TIMEOUT 3000

// Width and height of display, must be even. Height must be >= 32.
#define WIDTH 128
#define HEIGHT 64

// The i2c adress of the display
#define ADDRESS 0x3c

// This means I don't need to retype them it each time
#define cMessage "CPU:"
#define gMessage "GPU:"
#define mMessage "MEM:"
#define rMessage "RD:"
#define wMessage "WR:"
#define dMessage "DN:"
#define uMessage "UP:"

/* Put the pin numbers of pins that should be pulled
 *  high or low into the following arrays. This allows
 *  the display to be directly soldered to the board
 *  without  the need for any jumpers for power or ground. */
const uint8_t highPins[] = {10};
const uint8_t lowPins[] = {0, 11};

// Display object
Adafruit_SSD1306 oled(WIDTH, HEIGHT, &Wire, -1);

// Variables for data parsing
bool parsing = false;
char received;
char inputBuffer[100];
uint8_t bufferIndex = 0;
char intCode;

// This holds the data.
uint16_t C[WIDTH / 2];//CPU
uint16_t G[WIDTH / 2];//GPU
uint16_t M[WIDTH / 2];//RAM
uint16_t R[WIDTH / 2];//Read
uint16_t W[WIDTH / 2];//Write
uint16_t D[WIDTH / 2];//Download
uint16_t U[WIDTH / 2];//Upload

// These hold the maximum valuse which are used for graphing.
// These values are default, the python program can update them.
uint16_t rMax = 3000;
uint16_t wMax = 3000;
uint16_t dMax = 1000;
uint16_t uMax = 1000;

// This is used to convert numbers to strings
char conversionBuffer[10];

// Variables for the button and debounce
bool buttonState = false;
bool previousButtonState = false;
uint32_t pressStart = 0;
uint32_t pressDuration = 0;
uint32_t debounceStart = 0;
uint32_t debounceDuration = 0;
bool ignoreHold = false;

bool off = false;
bool timedOut = false;

// When did anything of importance happen? If its been too long the screen will turn off.
uint32_t timeOfEvent = 0;

// What is currently being displayed
int8_t displayMode = 0;


void setup() {

  // Comment out if not using a pico
  Wire.setSDA(SDA);
  Wire.setSCL(SCL);

  // These loops set pins high or low to be used as vcc or ground.
  for (int i = 0; i < sizeof(lowPins); i++) {
    pinMode(lowPins[i], OUTPUT);
    digitalWrite(lowPins[i], LOW);
  }
  for (int i = 0; i < sizeof(highPins); i++) {
    pinMode(highPins[i], OUTPUT);
    digitalWrite(highPins[i], HIGH);
  }

  pinMode(BUTTON, INPUT_PULLUP);

  Serial.begin(9600);

  // Without this, the oled will not start
  delay(200);

  // Start the oled, etc.
  oled.begin(SSD1306_SWITCHCAPVCC, ADDRESS);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.setTextColor(WHITE);
  // Render
  render();
}


void loop() {
  // Get incoming data.
  parse();

  // ! is used because the pin is in pullup mode.
  previousButtonState = buttonState;
  buttonState = !digitalRead(BUTTON);

  // In the event of a rollover (The millis function
  // rolls over after 49 days), this will solve any problems.
  // The only time these variables would be
  // ahead of millis() is if millis() rolled over recently.
  if (pressStart > millis()) {
    pressStart = millis();
  }
  if (debounceStart > millis()) {
    debounceStart = millis();
  }

  // This is used for power
  pressDuration = millis() - pressStart;
  // This is used for debounce
  debounceDuration = millis() - debounceStart;

  // If the button was pressed...
  if (buttonState && !previousButtonState) {
    
    // If debounce time is exceeded...
    if (debounceDuration > BOUNCE) {
      
      // Restart press and debounce timers.
      pressStart = millis();
      debounceStart = millis();

      // If you are off or timed out..
      if (off || timedOut) {
        // Clear the data, turn on, ignore hold
        // (it would be weird to press the button
        // to turn it on, only to have it turn off again),
        // and trigger an event.
        clearData();
        off = false;
        ignoreHold = true;
        event();
        
      } else {
        // This runs after the button was pressed
        // when the display was already on and not timed out.

        // Next display mode (it also rolls over)
        displayMode++;
        if (displayMode > 5) {
          displayMode = 0;
        }
        // Trigger an event
        event();
      }
    }
    
  // If the button was released...
  } else if (!buttonState && previousButtonState) {
    
    // Only if this is a valid release (not a bounce)...
    if (debounceDuration > BOUNCE) {
      // Restart the debounce timer
      debounceStart = millis();
      // and allow holds again.
      ignoreHold = false;
    }
    
  // If the button was held for the hold time, holds are not being ignored,
  // and the display is on and isn't timed out...
  } else if (buttonState && pressDuration >= HOLD && !ignoreHold && !off && !timedOut) {
    // Go back to show to the user that the device has gone back to how it was before the button was pressed
    displayMode--;
    if (displayMode < 0) {
      displayMode = 5;
    }
    // Trigger an event to prevent timeouts and update display.
    event();
    // Wait a moment so the user can see.
    delay(200);
    // Turn off and update display.
    off = true;
    render();
  }

  // This is just like we did with the button timers.
  if (timeOfEvent > millis()) {
    timeOfEvent = 0;
  }

  // Time out the display if it has been too long.
  if ((millis() - timeOfEvent) >= TIMEOUT) {
    timedOut = true;
    render();
  }
}


// This function resets the event timer, updates the display,
// and clears the data if it was previously timed out.
// It is called when the user does anything or when data is received from the computer.
void event() {
  if (timedOut) {
    clearData();
  }
  timedOut = false;
  timeOfEvent = millis();
  render();
}


// This function clears the data and is called when the device turns "on" or brought out of timeout.
void clearData() {
  for (uint16_t i = 0; i < WIDTH / 2; i++) {
    C[i] = 0;
    G[i] = 0;
    M[i] = 0;
    R[i] = 0;
    W[i] = 0;
    D[i] = 0;
    U[i] = 0;
  }
}


// This function gets the data.
void parse() {
  while (Serial.available() > 0) {
    // Store one character.
    received = Serial.read();
    // If it is <, begin parsing and shift the graphs.
    if (received == '<' && !parsing) {
      parsing = true;
      for (int i = WIDTH / 2 - 1; i > 0; i--) {
        C[i] = C[i - 1];
        G[i] = G[i - 1];
        M[i] = M[i - 1];
        R[i] = R[i - 1];
        W[i] = W[i - 1];
        D[i] = D[i - 1];
        U[i] = U[i - 1];
      }
    // Otherwise, if the device is parsing...
    } else if (parsing) {
      // If it is a digit, store it in the input buffer and increment the index.
      if (isDigit(received)) {
        inputBuffer[bufferIndex] = received;
        bufferIndex++;

      // if it is \, terminate the input buffer and stor the data in the respective array according to the intcode.
      } else if (received == '|') {
        inputBuffer[bufferIndex] = '\0';
        switch (intCode) {
          case 'C':
            C[0] = atoi(inputBuffer);
            break;
          case 'G':
            G[0] = atoi(inputBuffer);
            break;
          case 'M':
            M[0] = atoi(inputBuffer);
            break;
          case 'R':
            R[0] = atoi(inputBuffer);
            break;
          case 'W':
            W[0] = atoi(inputBuffer);
            break;
          case 'D':
            D[0] = atoi(inputBuffer);
            break;
          case 'U':
            U[0] = atoi(inputBuffer);
            break;
          case 'r':
            rMax = atoi(inputBuffer);
            break;
          case 'w':
            wMax = atoi(inputBuffer);
            break;
          case 'd':
            dMax = atoi(inputBuffer);
            break;
          case 'u':
            uMax = atoi(inputBuffer);
            break;
        }
      // If it is a >, stop parsing and trigger an event to prevent timeOut and update the display.
      } else if (received == '>') {
        parsing = false;
        //newData = true;
        event();
      // This means it is a symbol or letter
      } else {
        // Set the intcode to it, and reset the index.
        intCode = received;
        bufferIndex = 0;
      }
    }
  }
}


// This updates the display.
void render() {
  oled.clearDisplay();
  if (!off && !timedOut) {
    // Depending on the displaymode, different graphs will be drawn.
    switch (displayMode) {
      case 0:
        //ALL
        oled.fillRect(0, map(M[0], 0, 100, HEIGHT - 1, 0), 3, HEIGHT, WHITE);

        oled.drawLine(4, 0, 4, HEIGHT - 1, WHITE);
        oled.drawLine(WIDTH / 2 + 2, 0, WIDTH / 2 + 2, HEIGHT - 1, WHITE);
        oled.drawLine(5, HEIGHT / 2 - 1, WIDTH - 1, HEIGHT / 2 - 1, WHITE);

        graphRes(5, 0, WIDTH / 2 - 3, HEIGHT / 2 - 1, &C[0], 100, cMessage);
        graphRes(WIDTH / 2 + 3, 0, WIDTH / 2 - 3, HEIGHT / 2 - 1, &G[0], 100, gMessage);

        drawGraph(5, HEIGHT / 2, WIDTH / 2 - 3, HEIGHT / 2, &D[0], dMax);
        drawGraph(5, HEIGHT / 2, WIDTH / 2 - 3, HEIGHT / 2, &U[0], uMax);
        drawLabel(5, HEIGHT / 2, &D[0], dMessage);
        drawLabel(5, HEIGHT / 2 + 8, &U[0], uMessage);

        drawGraph(WIDTH / 2 + 3, HEIGHT / 2, WIDTH / 2 - 3, HEIGHT / 2, &R[0], rMax);
        drawGraph(WIDTH / 2 + 3, HEIGHT / 2, WIDTH / 2 - 3, HEIGHT / 2, &W[0], wMax);
        drawLabel(WIDTH / 2 + 3, HEIGHT / 2, &R[0], rMessage);
        drawLabel(WIDTH / 2 + 3, HEIGHT / 2 + 8, &W[0], wMessage);

        break;
      case 1:
        //CPU
        graphRes(0, 0, WIDTH, HEIGHT, &C[0], 100, cMessage);
        break;
      case 2:
        //Gpu
        graphRes(0, 0, WIDTH, HEIGHT, &G[0], 100, gMessage);
        break;
      case 3:
        //MEM
        graphRes(0, 0, WIDTH, HEIGHT, &M[0], 100, mMessage);
        break;
      case 4:
        //DISK
        graphRes(0, 0, WIDTH / 2, HEIGHT, &R[0], 1200, rMessage);
        graphRes(WIDTH / 2 + 1, 0, WIDTH / 2 - 1, HEIGHT, &W[0], 1200, wMessage);
        oled.drawLine(WIDTH / 2, 0, WIDTH / 2, HEIGHT - 1, WHITE);
        break;
      case 5:
        //NETWORK
        graphRes(0, 0, WIDTH / 2, HEIGHT, &D[0], 120, dMessage);
        graphRes(WIDTH / 2 + 1, 0, WIDTH / 2 - 1, HEIGHT, &U[0], 120, uMessage);
        oled.drawLine(WIDTH / 2, 0, WIDTH / 2, HEIGHT - 1, WHITE);
        break;
    }
  }
  oled.display();
}


// This function graphs a resource and writes the proper label.
void graphRes(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t* inputBuffer, uint16_t maxData, char message[]) {
  drawGraph(x, y, width, height, inputBuffer, maxData);
  drawLabel(x, y, inputBuffer, message);
}


// This function draws a label for a resource
void drawLabel(uint8_t x, uint8_t y, uint16_t* inputBuffer, char message[]) {
  ultoa(*inputBuffer, conversionBuffer, 10);
  oled.setCursor(x + 1, y + 1);
  oled.fillRect(x, y, (strlen(conversionBuffer) + strlen(message)) * 6 + 1, 9, BLACK);
  oled.print(message);
  oled.print(conversionBuffer);
}


// This function draws a graph.
void drawGraph(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t* inputBuffer, uint16_t maxData) {
  for (uint8_t i = 0; i < width / 2 - 1; i++) {
    oled.drawLine(map(i, 0, width / 2 - 1, x + width - 1, x), map(constrain(*(inputBuffer + i), 0, maxData), 0, maxData, y + height - 1, y), map(i + 1, 0, width / 2 - 1, x + width - 1, x), map(constrain(*(inputBuffer + i + 1), 0, maxData), 0, maxData, y + height - 1, y), WHITE);
  }
}

//TODO:
//var types (ints, consts etc.)
//Comment
//Test defined strings
