/**
   MozziScout2 is just like normal Scout2, except we generate _all_ of the tones on pin 9 (OCR1A).

   Originally by @todbot 12 Dec 2021
   Modified by @shamlian in 2025
*/

//TODO: Get analog reads back. Can I configure Mozzi to only use certain analog inputs? A0, A1, and A2 are in the key matrix.
#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE

#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/smoothsquare8192_int8.h>
#include <mozzi_midi.h> // for mtof()
#include <ADSR.h>
#include <Keypad.h>

// SETTINGS
#define BASE_OCTAVE 3
#define NUM_VOICES 6

const int OCTAVE_PIN = A4;
const int GLIDE_PIN = A5;

const byte ROWS = 5;
const byte COLS = 5;
byte key_indexes[ROWS][COLS] = {{1, 6, 11, 16, 21},
                                {2, 7, 12, 17, 22},
                                {3, 8, 13, 18, 23},
                                {4, 9, 14, 19, 24},
                                {5, 10, 15, 20, 25}};
byte rowPins[ROWS] = {7, 8, 14, 15, 16};
byte colPins[COLS] = {2, 3, 4, 5, 6};

Keypad keys = Keypad(makeKeymap(key_indexes), rowPins, colPins, ROWS, COLS);

#define CONTROL_RATE 64

Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE> myOscs[ NUM_VOICES ] = {
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
  Oscil<SMOOTHSQUARE8192_NUM_CELLS, AUDIO_RATE>(SMOOTHSQUARE8192_DATA),
};

// volume control envelopes
ADSR <CONTROL_RATE, AUDIO_RATE> myEnvs[NUM_VOICES];

void blink(int count = 2, int wait = 200) {
  while (count >= 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(wait);
    digitalWrite(LED_BUILTIN, LOW);
    delay(wait);
    count = count - 1;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  blink();

  for ( int i = 0; i < NUM_VOICES; i++) {
    myEnvs[i].setADLevels(255, 255);
    myEnvs[i].setTimes(20, 50, 20000, 50); 
  }

  startMozzi(); // start with default control rate of 64
}

void loop() {
  audioHook(); // required here
}

byte note_list[NUM_VOICES] = {0};

void updateControl() {
  String msg;

  if (keys.getKeys()) {
    for (int i = 0; i < LIST_MAX; i++) {   // Scan the whole key list.
      if ( keys.key[i].stateChanged ) {  // Only find keys that have changed state.
        byte note = 59 + (BASE_OCTAVE * 12) - 36 + keys.key[i].kchar;

        switch (keys.key[i].kstate) {  // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
          case PRESSED:
            msg = " PRESSED.";

            for (int i = 0; i < NUM_VOICES; i++) {
              if (note_list[i] == note) { // already pressed
                Serial.print("ALREADY PRESSED");
                myEnvs[i].noteOn();
                return;
              }
            }
            for(int i = 0; i < NUM_VOICES; i++) { 
              if (note_list[i] == 0) { // available
                note_list[i] = note;
                myEnvs[i].noteOn();
                myOscs[i].setFreq(mtof(note));
                break;
              }
            }
          break;
        case HOLD:
          msg = " HOLD.";
          break;

        case RELEASED:
          msg = " RELEASED.";

          for(int i=0; i< NUM_VOICES; i++) { 
            if( note == note_list[i] ) {
              note_list[i] = 0; // say its available
              myEnvs[i].noteOff(); 
            }
          }
          break;
        case IDLE:
          msg = " IDLE.";
          break;
        }
        Serial.print("Key ");
        Serial.print((byte)keys.key[i].kchar);
        Serial.println(msg);
      }
    }
  }

  for (int i = 0; i < NUM_VOICES; i++) {
    myEnvs[i].update();
    if( myEnvs[i].adsr_playing == false ) { 
      note_list[i] = 0; // just in case
    }
  }
  
}
AudioOutput_t updateAudio() {
  long asig = (long) 0;
  for ( int i = 0; i < NUM_VOICES; i++) {
    //    asig += myOscs[i].next();
    asig += myOscs[i].next() * myEnvs[i].next();
  }
  return MonoOutput::fromAlmostNBit(19, asig);
}
