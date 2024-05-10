#include <RTClib.h>
#include <Stepper.h>
#include <Wire.h>

// Port H - Stepper motor
volatile unsigned char* DDRH_REG = (volatile unsigned char*) 0x101;
volatile unsigned char* PORTH_REG = (volatile unsigned char*) 0x102;

// Port E - Start, Stop button Port
volatile unsigned char* DDRE_REG = (volatile unsigned char*) 0x21;
volatile unsigned char* PORTE_REG = (volatile unsigned char*) 0x2C;

// Port B - LED Ports
volatile unsigned char *DDRB_REG = (volatile unsigned char*)0x24;
volatile unsigned char *PORTB_REG = (volatile unsigned char*)0x25;

// USART Registers
#define TBE 0x20
volatile unsigned char *_UCSR0A = (unsigned char *)0x00C0; // USART control status registers
volatile unsigned char *_UCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *_UCSR0C = (unsigned char *)0x00C2;
volatile unsigned int *_UBRR0 = (unsigned int *)0x00C4;
volatile unsigned char *_UDR0 = (unsigned char *)0x00C6; // USART buffer

// Realtime Clock
RTC_DS1307 rtc;

// State flag declaration
enum states{DISABLED, IDLE, ERROR, RUNNING};
volatile enum states coolerState;
volatile bool stateChanged = 0;

// Fan flag
volatile bool fanOn = false;

void setup() {
  // put your setup code here, to run once:
  coolerState = IDLE;

  // Initialize UART for Serial Out
  U0init(9600);

  //PH5 - PH6 in OUTPUT mode
  *DDRH_REG |= (0x1 << 5); // pin D8
  *DDRH_REG |= (0x1 << 6); // pin D9

  //PB4 - PB7 in OUTPUT mode
  *DDRB_REG |= (0x1 << 4);
  *DDRB_REG |= (0x1 << 5);
  *DDRB_REG |= (0x1 << 6);
  *DDRB_REG |= (0x1 << 7);

  // Start, Stop interrupt initializer
  *DDRE_REG |= (0x0 << 4); // Pin D2 (Port E4) to INPUT mode
  attachInterrupt(digitalPinToInterrupt(2), startStopInterrupt, RISING);

  // Real Time Clock
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

}

void loop() {

  // Report Humidity, Temperature
  float temperature;
  float humidity;

  if (coolerState != DISABLED){

    temperature = 0.0;
    humidity = 0.0;

    // Output to LCD

  }

  // Handle Vent Angle change
  bool upButton = 0;
  bool downButton = 0;
  if (upButton){

    ventAngle(0);

  } else if (downButton) {

    ventAngle(1);

  }

  // Fan
  setFan(fanOn);

  // State

  if (stateChanged) {

    printStatus();
    stateChanged = 0;

  }

  switch(coolerState){

    case DISABLED:
      disabledState();
      break;

    case IDLE:
      idleState();
      break;

    case ERROR:
      errorState();
      break;

    case RUNNING:
      runningState(temperature);
      break;

    default:
      transitionTo(ERROR);
  }

}

void disabledState(){

  // Fan off
  fanOn = false;

  // Yellow LED ON
  setLED(1, 0, 0, 0);

}

void idleState(){

  // Green LED ON
  setLED(0, 1, 0, 0);

  // Monitor water level
  bool waterLevelCritical = false;

  if (waterLevelCritical) {

    transitionTo(ERROR);

  } 

}

void errorState(){

  // Turn off Fan
  fanOn = false;

  // Display Error to LCD


  // Red LED ON
  setLED(0, 0, 1, 0);

}

void runningState(float temperature){

  int threshold = 1;

  // Run Cooler
  if (temperature >= threshold){

    fanOn = true;

  }

  // Check water level
  bool waterLevelCritical = false;

  if (waterLevelCritical) {

    transitionTo(ERROR);

  } else if (temperature <= threshold) {

    transitionTo(IDLE);

  }

}

void transitionTo(enum states nextState){
  
  // Change state flag
  coolerState = nextState;
  stateChanged = 1;

  printStatus();

}

void printStatus(){

  char nextStateString[9];

  switch(coolerState){

      case DISABLED:
        strcpy(nextStateString, "DISABLED"); 
        break;

      case IDLE:
        strcpy(nextStateString, "IDLE");
        break;

      case ERROR:
        strcpy(nextStateString, "ERROR");
        break;

      case RUNNING:
        strcpy(nextStateString, "RUNNING");
        break;
  }

  char message[50]; // Increase the size of message array to accommodate the string
  // Use snprintf to format the string and store it in message array
  int messageLength = snprintf(message, sizeof(message), "State: %s", nextStateString);
  printToSerial(message, messageLength);

  // Report Timestamp
  printTime();

  // Report vent angle


}

void setFan(bool on){

  if(on){

    *PORTH_REG |= (1 << 5);  //set pin 8
    *PORTH_REG &= ~(1 << 6); //clear pin 9


  } else {

    *PORTH_REG &= ~(1 << 5); //clear pin 5
    *PORTH_REG &= ~(1 << 6); //clear pin 6

  }

}

void setLED(const bool Y, const bool G, const bool R, const bool B){

  *PORTB_REG &= ((Y == 1 ? 0x1 : 0x0) << 7);
  *PORTB_REG &= ((G == 1 ? 0x1 : 0x0) << 6);
  *PORTB_REG &= ((R == 1 ? 0x1 : 0x0) << 5);
  *PORTB_REG &= ((B == 1 ? 0x1 : 0x0) << 4);

  *PORTB_REG |= (Y == 1 ? 0x1 : 0x0) << 7;
  *PORTB_REG |= (G == 1 ? 0x1 : 0x0) << 6;
  *PORTB_REG |= (R == 1 ? 0x1 : 0x0) << 5;
  *PORTB_REG |= (B == 1 ? 0x1 : 0x0) << 4;

}

void ventAngle(bool direction){

  const int stepsPerRevolution = 2038;  // Defines the number of steps per rotation
  // Creates an instance of stepper class
  // Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
  Stepper myStepper = Stepper(stepsPerRevolution, 40, 42, 44, 46);

  myStepper.setSpeed(5);

  if(direction){

    myStepper.step(stepsPerRevolution/4);

  } else if (!direction){

    myStepper.step(-1*stepsPerRevolution/4);

  }

}

void startStopInterrupt(){
  
  coolerState = (coolerState == DISABLED ? IDLE : DISABLED);
  stateChanged = 1;

}

void printTime(){

   DateTime now = rtc.now();

  int hour = now.hour();
  int min = now.minute();
  int sec = now.second();

  unsigned char timeString[16];
  sprintf(timeString, "Time: %d:%d:%d", hour, min, sec);
  printToSerial(timeString, 16);

}

void U0init(unsigned long U0baud) {

  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1); //baud rate operation
 
  *_UCSR0A = 0x20; // double-speed operation
  *_UCSR0B = 0x18; // enable transmitter and receiver
  *_UCSR0C = 0x06; // 8 bit data communication
  *_UBRR0 = tbaud; // set baud rate

}

void printToSerial(char message[], int size){

  for (int i = 0; i < size; i++) {

    U0putchar((unsigned char)message[i]);

  }

  U0putchar('\n');

}

void U0putchar(unsigned char U0pdata) {

  while (!(*_UCSR0A & TBE)); // Transmitter Buffer Empty

  *_UDR0 = U0pdata; // Put data to buffer

}

