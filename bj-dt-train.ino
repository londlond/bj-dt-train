 #include <UIPEthernet.h>
 #include <TimerOne.h> 

// Config Network 
byte mac[] = { 0x54, 0x34, 0x41, 0x3f, 0x4d, 0x31 };

IPAddress ip(192, 168, 0, 111);
EthernetServer server(80);

// Config Stepper Pins
#define enablePin 6
#define stepPin 7
#define dirPin 8

#define STEPS_FOR_1ML 270 // Steps needed to push 1ml with syringe

// Config Piezo Pin
#define piezoPin 5

// Config Lock Pin (attach a relais)
#define lockPin 4

// Config Ultrasonic sensor
#define trigPin 9  // Pin 9 trigger output
#define echoPin 2   // Pin 2 Echo input
#define echo_int 0  // Interrupt id for echo pulse

// Timer interrupt config (used for ultrasonic sensor)
#define TIMER_US 50                      // 50 uS timer duration 
#define TICK_COUNTS 4000                 // 200 mS worth of timer ticks



// Config BJ Logic
unsigned long tick = 0; // used to store 'time' 
unsigned long MIN_HOLD = 100; // minimum hold time (hold in mouth)
unsigned long DT_HOLD = 3000; // same for DT
unsigned int CUM_NORMAL = 2; // number of BJs head movements needed to trigger
unsigned int CUM_DT = 0; // same for DT
unsigned int PRECUM_TRIGGER = 2; // this has to be lower than CUM_NORMAL, and activates precum-logic
unsigned int ROUNDS_TOTAL = 1; // number of rounds to train, if this is reaced, lock opens (unlocks)

// I recommend hardcoding small and safe times, rounds and so on 
// -> so in case you make a mistake (for example accidently reset the power to device),
// you don't have to reach a 400 times BJ limit to unlock again

// more config
float ML_PRECUM = 0.5; // Amount of 'Precum' ejected
float ML_CUM = 6.0; // Normal cumming amount (everything in milliliters)
float RANGE_NORMAL = 12.0; // distance to the sensor, so it triggers a normal BJ
float RANGE_DT = 9.0; // same, but for a DT to recognize
float RANGE_NORMAL_BACK = 15.0; // you have to move away from the sensor so it counts (used for both BJ & DT)


// Global Variables
unsigned int CounterNormalBjs = 0; // Store the current progress of BJs
unsigned int CounterDtBjs = 0; // same for DTs
unsigned int current_round = 0; // and the current round

// States for the statemachine
#define initBj 0
#define startNormBj 1
#define startDtBj 2
#define holdNormBj 3
#define holdDtBj 4
#define leakPrecum 5
#define ejacCum 6

// Global variables for the ultrasonic sensor
volatile long echo_start = 0;            // Records start of echo pulse 
volatile long echo_end = 0;              // Records end of echo pulse
volatile long echo_duration = 0;         // Duration - difference between end and start
volatile int trigger_time_count = 0;     // Count down counter to trigger pulse time
volatile long range_flasher_counter = 0; // Count down counter for flashing distance LED


void setup() {
  
  // Ultrasonic Pins
  pinMode(trigPin, OUTPUT);  // Trigger pin set to output
  pinMode(echoPin, INPUT);   // Echo pin set to input

  // Stepper Driver Pins
  pinMode(enablePin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  // Enable Driver
  digitalWrite(enablePin, LOW);

  // Relais
  pinMode(lockPin, OUTPUT);

  Timer1.initialize(TIMER_US);                        // Initialise timer 1
  Timer1.attachInterrupt(timerIsr);                 // Attach interrupt to the timer service routine 
  attachInterrupt(echo_int, echo_interrupt, CHANGE);  // Attach interrupt to the sensor echo input
  
  Serial.begin(115200);
  //Serial.println(F("WEB SERVER "));
  Serial.println();
  Ethernet.begin(mac, ip);
  server.begin();
  //Serial.print(F("IP Address: "));
  Serial.println(Ethernet.localIP());
  
  playStartUpSound();
  unlock(); // for safety 
}

String readString; // stores the http request
void loop() {
  float dist = float(echo_duration) / 58.0;
  //Serial.println(dist); // Print the distance in centimeters
  checkSonic(dist);
  checkCounters(CounterNormalBjs, CounterDtBjs);
  EthernetClient client = server.available();
  if (client) {
    //Serial.println("-> New Connection");
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      //Serial.println("connected");
      if (client.available()) {
        char c = client.read();
        //read char by char HTTP request
        if (readString.length() < 250) {
          //store characters to string
          readString += c;
         }
        if (c == '\n' && currentLineIsBlank) { // If http request has ended
          //Serial.print(readString);
          // check url for button-commands
          if (readString.indexOf(F("?btn1")) > 0) {
            pushMl(1.0, false);
          }
          else if (readString.indexOf(F("?btn2")) > 0) {
            pushMl(10.0, false);
          }
          else if (readString.indexOf(F("?btn3")) > 0) {
            pushMl(1.0, true);
          }
          else if (readString.indexOf(F("?btn4")) > 0) {
            pushMl(10.0, true);
          }
          else if (readString.indexOf(F("?btn5")) > 0) {
            blowLoad(5);
          }
          else if (readString.indexOf(F("?btn6")) > 0) {
            lock();
          }
          else if (readString.indexOf(F("?btn7")) > 0) {
            unlock();
          }

          // check url for config values and extract them if needed
          if (readString.indexOf(F("RANGE_NORMAL")) > 0) {
            String tmp = getValAfter(F("RANGE_NORMAL"));
            tmp.replace(',','.');
            RANGE_NORMAL = tmp.toFloat();
          }
          if (readString.indexOf(F("RANGE_NORMAL_BACK")) > 0) {
            Serial.println(getValAfter(F("RANGE_NORMAL_BACK")));
            String tmp = getValAfter(F("RANGE_NORMAL_BACK"));
            tmp.replace(',','.');
            RANGE_NORMAL_BACK = tmp.toFloat();
          }
          if (readString.indexOf(F("RANGE_DT")) > 0) {
            Serial.println(getValAfter(F("RANGE_DT")));
            String tmp = getValAfter(F("RANGE_DT"));
            tmp.replace(',','.');
            RANGE_DT = tmp.toFloat();
          }
          if (readString.indexOf(F("DT_HOLD")) > 0) {
            String tmp = getValAfter(F("DT_HOLD"));
            DT_HOLD = tmp.toInt();
          }
          if (readString.indexOf(F("ML_PRECUM")) > 0) {
            String tmp = getValAfter(F("ML_PRECUM"));
            tmp.replace(',','.');
            ML_PRECUM = tmp.toFloat();
          }
          if (readString.indexOf(F("ML_CUM")) > 0) {
            String tmp = getValAfter(F("ML_CUM"));
            tmp.replace(',','.');
            ML_CUM = tmp.toFloat();
          }
          if (readString.indexOf(F("PRECUM_TRIGGER")) > 0) {
            String tmp = getValAfter(F("PRECUM_TRIGGER"));
            PRECUM_TRIGGER = tmp.toInt();
          }
          if (readString.indexOf(F("CUM_NORMAL")) > 0) {
            String tmp = getValAfter(F("CUM_NORMAL"));
            CUM_NORMAL = tmp.toInt();
          }
          if (readString.indexOf(F("CUM_DT")) > 0) {
            String tmp = getValAfter(F("CUM_DT"));
            CUM_DT = tmp.toInt();
          }
          if (readString.indexOf(F("ROUNDS_TOTAL")) > 0) { // reset the current state
            String tmp = getValAfter(F("ROUNDS_TOTAL"));
            ROUNDS_TOTAL = tmp.toInt();
            current_round = 0;
            CounterNormalBjs = 0;
            CounterDtBjs = 0;
          }
          
          readString = "";

          // Messy but necessary since we don't have an SD-Card to store templates :(
          client.println(F("HTTP/1.1 200 OK")); //send new page
          client.println(F("Content-Type: text/html"));
          client.println();
          client.println(F("<html><head></head><body>"));
          // Build message with current state & values
          String msg = "N: " + String(CounterNormalBjs,DEC) + "/" + String(CUM_NORMAL,DEC) + " - " + String(CounterDtBjs,DEC) + "/" + String(CUM_DT,DEC) + " , " + String(CounterNormalBjs,DEC) + "/" + String(PRECUM_TRIGGER,DEC) + " Rounds " + String(current_round, DEC) + "/" + String(ROUNDS_TOTAL,DEC);
          client.println(msg);
          client.println(F("<br>"));
          client.println(F("<form action='/' method=get >"));
          client.println(F("<table><thead><tr><th>Cfg</th><th>Val</th><th>Desc</th></tr></thead><tbody><tr><td>Range Normal</td><td><input type='number' name=RANGE_NORMAL step=0.1 value="));
          client.println(RANGE_NORMAL);
          client.println(F("></td><td>if &lt;, trigger<br></td></tr><tr><td>Range Normal Back</td><td><input type='number' name=RANGE_NORMAL_BACK step=0.1 value="));
          client.println(RANGE_NORMAL_BACK);
          client.println(F("></td><td></td></tr><tr><td>Range DT</td><td><input type='number' name=RANGE_DT step=0.1 value="));
          client.println(RANGE_DT);
          client.println(F("></td><td></td></tr><tr><td>DT Hold</td><td><input type='number' name=DT_HOLD value="));
          client.println(DT_HOLD);
          client.println(F("></td><td>DT hold time</td></tr><tr><td>Precum amount</td><td><input type='number' step=0.1 name=ML_PRECUM value="));
          client.println(ML_PRECUM);
          client.println(F("></td><td></td></tr><tr><td>Cum amount</td><td><input type='number' name=ML_CUM step=0.1 value="));
          client.println(ML_CUM);
          client.println(F("></td><td></td></tr><tr><td colspan='3'>Trigger Cfg</td></tr><tr><td>Precum Trigger</td><td colspan='2'><input type='number' name=PRECUM_TRIGGER value="));
          client.println(PRECUM_TRIGGER);
          client.println(F("></td></tr></td></tr><tr><td colspan='3'>Session Cfg</td></tr><tr><td>Cum Normal</td><td colspan='2'><input type='number' name=CUM_NORMAL value="));
          client.println(CUM_NORMAL);
          client.println(F("></td></tr><tr><td>Cum DT</td><td colspan='2'><input type='number' name=CUM_DT value="));
          client.println(CUM_DT);
          client.println(F("</td></tr><tr><td>Rounds</td><td colspan='2'><input type='number' name=ROUNDS_TOTAL value="));
          client.println(ROUNDS_TOTAL);
          client.println(F("</td></tr></tbody></table>"));
          client.println(F("<br>"));   
          client.println(F("<input type=submit name='submit' value='Update Config'>"));
          client.println(F("</form>"));
          client.println(F("<br>"));

          client.println(F("<button><a href=\"/?btn1\"\">+ 1ml</a></button>"));
          client.println(F("<button><a href=\"/?btn2\"\">+ 10ml</a></button>"));
          client.println(F("<button><a href=\"/?btn3\"\">- 1ml</a></button>"));
          client.println(F("<button><a href=\"/?btn4\"\">- 10ml</a></button>"));
          client.println(F("<br>"));
          client.println(F("<button><a href=\"/?btn5\"\">Blow Load</a></button>"));
          client.println(F("<br>"));
          client.println(F("<button><a href=\"/?btn6\"\">Lock</a></button>"));
          client.println(F("<button><a href=\"/?btn7\"\">Unlock</a></button>"));

          client.println(F("</body>"));
          client.println(F("</html>"));

          delay(1);
          client.stop();
        }
      }
    }
  }
  delay(100);
}


// returns the string after a config value (delimiter is '&')
String getValAfter(String searchText)
{
  int pos = readString.indexOf(searchText);
  if (pos != -1)
  {
    pos += searchText.length() + 1; // position after '='
    // Search for delimiter
    int endPos = readString.indexOf("&", pos);
    if (endPos != -1)
    {
      return readString.substring(pos, endPos);
    }
    return "";
  }
  return "";
}


void lock()
{
  digitalWrite(lockPin, HIGH);
}

void unlock()
{
  digitalWrite(lockPin, LOW);
}

// copied from official example
// --------------------------
// timerIsr() 50uS second interrupt ISR()
// Called every time the hardware timer 1 times out.
// --------------------------
void timerIsr()
{
    trigger_pulse();      // Schedule the trigger pulses
}

// --------------------------
// trigger_pulse() called every 50 uS to schedule trigger pulses.
// Generates a pulse one timer tick long.
// Minimum trigger pulse width for the HC-SR04 is 10 us. This system
// delivers a 50 uS pulse.
// --------------------------
void trigger_pulse()
{
      static volatile int state = 0;                 // State machine variable

      if (!(--trigger_time_count))                   // Count to 200mS
      {                                              // Time out - Initiate trigger pulse
         trigger_time_count = TICK_COUNTS;           // Reload
         state = 1;                                  // Changing to state 1 initiates a pulse
      }
    
      switch(state)                                  // State machine handles delivery of trigger pulse
      {
        case 0:                                      // Normal state does nothing
            break;
        
        case 1:                                      // Initiate pulse
           digitalWrite(trigPin, HIGH);              // Set the trigger output high
           state = 2;                                // and set state to 2
           break;
        
        case 2:                                      // Complete the pulse
        default:      
           digitalWrite(trigPin, LOW);               // Set the trigger output low
           state = 0;                                // and return state to normal 0
           break;
     }
}

// --------------------------
// echo_interrupt() External interrupt from HC-SR04 echo signal. 
// Called every time the echo signal changes state.
//
// Note: this routine does not handle the case where the timer
//       counter overflows which will result in the occassional error.
// --------------------------
void echo_interrupt()
{
  switch (digitalRead(echoPin))                     // Test to see if the signal is high or low
  {
    case HIGH:                                      // High so must be the start of the echo pulse
      echo_end = 0;                                 // Clear the end time
      echo_start = micros();                        // Save the start time
      break;
      
    case LOW:                                       // Low so must be the end of hte echo pulse
      echo_end = micros();                          // Save the end time
      echo_duration = echo_end - echo_start;        // Calculate the pulse duration
      break;
  }
}


// Detects Blows & DTs and counts them
unsigned char currentState = initBj;
void checkSonic(float dist)
{
  if (dist > 50.0) // ignore bounces and mismeasurements (0,5m only interesting)
  {
    return;
  }
  // Check if next event can be detected
  unsigned long tock = millis();
  unsigned char nextState = currentState; // Stay in same state if nothing happens

  // Check State
  //Serial.println(currentState);
  switch (currentState)
  {
    case initBj: {
      //Serial.println("initBj");
      if (dist <= RANGE_DT)
      {
        nextState = startDtBj;
      }
      else if (dist <= RANGE_NORMAL) // Start detection of normal bj
      {
        nextState = startNormBj;
      }
      tick = millis(); // Reset Time, since nothing was detected
      break;
    }
    case startNormBj: {
      //Serial.println("startNormBj");
      if (dist <= RANGE_DT) // switch to DT mode
      {
        nextState = startDtBj;
      }
      else if (dist <= RANGE_NORMAL && ((tock - tick) > MIN_HOLD)) // check if it was long enougth around pos
      {
        playNormTone();
        nextState = holdNormBj; // Next State
      }
      break;
    }
    case startDtBj: {
      //Serial.println("startDtBj");
      if (dist <= RANGE_DT && ((tock - tick) > DT_HOLD)) // pos ok and long enought hold in
      {
        playDtTone();
        nextState = holdDtBj;
      }
      else if (dist > RANGE_DT) // pos not ok -> reset to init and reset Time
      {
        playDtErrorSound();
        nextState = initBj;
        tick = millis();
      }
      break;
    }
    case holdNormBj: {
      //Serial.println("holdNormBj");
      if (dist > RANGE_NORMAL_BACK)
      {
        CounterNormalBjs += 1; // count bj
        tick = millis(); // reset Time
        nextState = initBj;
      }
      break;
    }
    case holdDtBj: {
      //Serial.println("holdDtBj");
      if (dist > RANGE_NORMAL_BACK)
      {
        CounterDtBjs += 1; // count bj
        tick = millis(); // reset Time
        nextState = initBj;
      }
      break;
    }
    default: {
      break;
    } 
  }
  currentState = nextState;
  //String msg = "N: " + String(CounterNormalBjs,DEC) + "/" + String(CUM_NORMAL,DEC) + " - " + String(CounterDtBjs,DEC) + "/" + String(CUM_DT,DEC) + " , " + String(CounterNormalBjs,DEC) + "/" + String(PRECUM_TRIGGER,DEC);
  //Serial.println(msg);

}

//checks the counters and executes action if limits are reached
bool precumDone = false;
void checkCounters(unsigned int normalBjs, unsigned int dtBjs) 
{
  if (!precumDone && normalBjs == PRECUM_TRIGGER)
  {
    // precum
    pushMl(ML_PRECUM, true);
    precumDone = true;
  }
  else if (normalBjs >= CUM_NORMAL && dtBjs >= CUM_DT)
  {
    // BLow Load
    blowLoad(ML_CUM);
    // Reset for next round
    precumDone = false;
    CounterNormalBjs = 0;
    CounterDtBjs = 0;
    current_round += 1;
    if (current_round == ROUNDS_TOTAL)
    {
      // finished
      unlock();
    }
  }
}

void pushMl(float ml, bool push)
{
  int stepCounter;
  int steps = STEPS_FOR_1ML*ml; // calculate steps needed
  if (!push)
  {
    digitalWrite(dirPin,LOW); // im Uhrzeigersinn
  }
  else
  {
    digitalWrite(dirPin,HIGH);
  }
  
  for(stepCounter = 0; stepCounter < steps; stepCounter++) {
    digitalWrite(stepPin,HIGH);
    delayMicroseconds(500);
    digitalWrite(stepPin,LOW);
    delayMicroseconds(500);
  }  
}

void blowLoad(float ml) {
  // do it in random cycles for realness
  int cycles_total = random(5,15);
  for (int cycles = 0; cycles < cycles_total; cycles++)
  {
    pushMl(ml/cycles_total, true);
    // simulate realness with random times between cycles
    int randomness = random(2,15);
    delay(150*randomness);
  }
}

void playDtTone()
{
  tone(piezoPin, 800); 
  delay(80);
  noTone(piezoPin);
  delay(80);
  tone(piezoPin, 1000); 
  delay(80);
  noTone(piezoPin);
}

void playNormTone()
{
  tone(piezoPin, 600); 
  delay(80);
  noTone(piezoPin);
}

void playDtErrorSound()
{
  tone(piezoPin, 200); 
  delay(120);
  noTone(piezoPin);
  delay(120);
  tone(piezoPin, 200); 
  delay(120);
  noTone(piezoPin);

}

void playStartUpSound()
{
  tone(piezoPin, 1000); 
  delay(700);
  noTone(piezoPin);
}
