#if 1 // Change to 1 to enable this code (must enable ONE user*.cpp only!)

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <Adafruit_NeoPixel.h>
#include "globals.h"

#define LED_PIN    8
#define LED_COUNT  4
#define PIR_PIN    2

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

int currentLED = 0; // Global variable to keep track of the current LED
unsigned long lastSoundTime = 0; // Global variable to keep track of the last sound play time
const unsigned long soundInterval = 60000; //interval in milliseconds (60000 = 1 minute)

const unsigned long flickerInterval = 180; // Flicker interval in milliseconds for red laughter
static unsigned long lastFlickerTime = 0;
static bool ledsAreRed = false; // Boolean to track if LEDs are red
static uint8_t priorPIRState = LOW; // Track previous PIR state

#define WAV_BUFFER_SIZE    256
static uint8_t     wavBuf[2][WAV_BUFFER_SIZE];
static File        wavFile;
static bool        playing = false; // currently playing sound
static int         remainingBytesInChunk;
static uint8_t     activeBuf;
static uint16_t    bufIdx, bufEnd, nextBufEnd;
static bool        startWav(const char *filename);
static void        wavOutCallback(void);
static uint32_t    wavEventTime; // WAV start or end time, in ms

// Static list of WAV filenames
static const char *wavFiles[] = {
  "sounds/laugh1.wav",
  "sounds/laugh2.wav",
  "sounds/laugh3.wav"
};
static int currentFileIndex = 0;
static const int numFiles = sizeof(wavFiles) / sizeof(wavFiles[0]);

void user_setup(void) {
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
  arcada.arcadaBegin();
  arcada.filesysBegin();
  pinMode(PIR_PIN, INPUT); // Initialize PIR sensor pin
  priorPIRState = digitalRead(PIR_PIN); // Read initial PIR state
}

// Function to generate a warm white-yellow flame color
uint32_t flameWhiteYellow() {
  int red = random(240, 255);       // Very high red for warm white-yellow
  int green = random(200, 255);     // High green, slightly lower than red
  int blue = random(0, 20);         // Minimal blue to keep it warm
  return strip.Color(red, green, blue);
}

// Function to generate a warm yellow-orange flame color
uint32_t flameYellowOrange() {
  int red = 255;                    // Max red
  int green = random(150, 200);     // Strong green, not too high
  int blue = random(0, 10);         // Minimal blue to keep the hue warm
  return strip.Color(red, green, blue);
}

// Function to generate a warm orange-red flame color
uint32_t flameOrangeRed() {
  int red = 255;                    // Max red
  int green = random(50, 100);      // Mid-range green for orange tone
  int blue = 0;                     // No blue for deep orange-red
  return strip.Color(red, green, blue);
}

// Function to generate a warm deep red flame color
uint32_t flameRed() {
  int red = random(150, 255);       // High red for warmth
  int green = random(0, 50);        // Minimal green to keep it dark red
  int blue = 0;                     // No blue to avoid cooling the hue
  return strip.Color(red, green, blue);
}

// Array of functions with increased probability for yellow-orange and white-yellow
uint32_t (*flameFunctions[])() = {
  flameWhiteYellow, flameYellowOrange,
  flameWhiteYellow, flameYellowOrange,
  flameOrangeRed, flameRed,
  flameYellowOrange, flameYellowOrange
};

void user_loop(void) {
  unsigned long currentTime = millis();
  uint8_t newPIRState = digitalRead(PIR_PIN); // Read current PIR state

  if (newPIRState != priorPIRState) {
    Serial.println(newPIRState == HIGH ? "PIR HIGH" : "PIR LOW");
  }

  if (playing) {
    if (currentTime - lastFlickerTime >= flickerInterval) {
      lastFlickerTime = currentTime;
      ledsAreRed = !ledsAreRed; // Toggle the boolean

      // Set all LEDs based on the boolean
      for (int i = 0; i < strip.numPixels(); i++) {
        if (ledsAreRed) {
          strip.setPixelColor(i, strip.Color(255, 0, 0)); // Red color
        } else {
          strip.setPixelColor(i, strip.Color(0, 0, 0)); // Turn off
        }
      }
      strip.show(); // Update strip with new contents
    }
  } else {
    // Pick a random color function to get the warm flame effect
    int colorIndex = random(0, 8);  // Choose from 0 to 7
    strip.setPixelColor(currentLED, flameFunctions[colorIndex]());

    strip.show();  // Update strip with new contents

    // Move to the next LED, wrapping around if necessary
    currentLED = (currentLED + 1) % strip.numPixels();

    // Check if PIR detects motion (transition from LOW to HIGH)
    if (newPIRState == HIGH && priorPIRState == LOW) {
      // Motion detected, check if cooldown period has passed
      if (currentTime - lastSoundTime >= soundInterval) {
        Serial.println("Motion detected - Playing sound");
        // Play the next .wav file in the list
        if (startWav(wavFiles[currentFileIndex])) {
          lastSoundTime = currentTime; // Update the last sound play time
          currentFileIndex = (currentFileIndex + 1) % numFiles; // Move to the next file
        } else {
          Serial.println("Failed to play sound");
        }
      } else {
        Serial.println("Motion detected but cooldown active");
      }
    }
  }

  priorPIRState = newPIRState; // Update the previous PIR state
}

static uint16_t readWaveData(uint8_t *dst) {
  if (remainingBytesInChunk <= 0) {
    // Read next chunk
    struct {
      char     id[4];
      uint32_t size;
    } header;
    for (;;) {
      if (wavFile.read(&header, 8) != 8) return 0;
      if (!strncmp(header.id, "data", 4)) {
        remainingBytesInChunk = header.size;
        break;
      }
      if (!wavFile.seekCur(header.size)) { // If not "data" then skip
        return 0; // Seek failed, return invalid count
      }
    }
  }

  int16_t bytesRead = wavFile.read(dst, min(WAV_BUFFER_SIZE, remainingBytesInChunk));
  if (bytesRead > 0) remainingBytesInChunk -= bytesRead;
  return bytesRead;
}

// Partially swiped from Wave Shield code.
// Is pared-down, handles 8-bit mono only to keep it simple.
static bool startWav(const char *filename) {
  Serial.println("Starting WAV, filename: " + String(filename));
  wavFile = arcada.open(filename);
  if (!wavFile) {
    Serial.println("Failed to open WAV file");
    return false;
  }
  Serial.println("Opened WAV file");

  union {
    struct {
      char     id[4];
      uint32_t size;
      char     data[4];
    } riff;  // riff chunk
    struct {
      uint16_t compress;
      uint16_t channels;
      uint32_t sampleRate;
      uint32_t bytesPerSecond;
      uint16_t blockAlign;
      uint16_t bitsPerSample;
      uint16_t extraBytes;
    } fmt; // fmt data
  } buf;

  uint16_t size;
  if ((wavFile.read(&buf, 12) == 12)
    && !strncmp(buf.riff.id, "RIFF", 4)
    && !strncmp(buf.riff.data, "WAVE", 4)) {
    // next chunk must be fmt, fmt chunk size must be 16 or 18
    if ((wavFile.read(&buf, 8) == 8)
      && !strncmp(buf.riff.id, "fmt ", 4)
      && (((size = buf.riff.size) == 16) || (size == 18))
      && (wavFile.read(&buf, size) == size)
      && ((size != 18) || (buf.fmt.extraBytes == 0))) {
      if ((buf.fmt.channels == 1) && (buf.fmt.bitsPerSample == 8)) {
        Serial.printf("Samples/sec: %d\n", buf.fmt.sampleRate);
        bufEnd = readWaveData(wavBuf[0]);
        if (bufEnd > 0) {
          // Initialize A/D, speaker and start timer
          analogWriteResolution(8);
          analogWrite(A0, 128);
          analogWrite(A1, 128);
          arcada.enableSpeaker(true);
          wavEventTime = millis(); // WAV starting time
          bufIdx       = 0;
          playing      = true;
          arcada.timerCallback(buf.fmt.sampleRate, wavOutCallback);
          nextBufEnd   = readWaveData(wavBuf[1]);
        }
        Serial.println("WAV started");
        return true;
      } else {
        Serial.println("Only 8-bit mono WAVs are supported");
      }
    } else {
      Serial.println("WAV uses compression or other unrecognized setting");
    }
  } else {
    Serial.println("Not WAV file");
  }
  Serial.println("Closing WAV file as failed");

  wavFile.close();
  return false;
}

static void wavOutCallback(void) {
  const float volumeScale = 0.35; // Adjust this value to set the volume (0.0 to 1.0)
  uint8_t n = wavBuf[activeBuf][bufIdx];
  n = static_cast<uint8_t>(n * volumeScale); // Scale the sample value to reduce volume

  analogWrite(A0, n);
  analogWrite(A1, n);

  if (++bufIdx >= bufEnd) {
    if (nextBufEnd <= 0) {
      arcada.timerStop();
      arcada.enableSpeaker(false);
      playing      = false;
      wavEventTime = millis(); // Same var now holds WAV end time
      return;
    }
    bufIdx     = 0;
    bufEnd     = nextBufEnd;
    nextBufEnd = readWaveData(wavBuf[activeBuf]);
    activeBuf  = 1 - activeBuf;
  }
}

#endif
