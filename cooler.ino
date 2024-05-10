#include <DHT11.h>
#include <RTClib.h>
#include <Stepper.h>
#include <Wire.h>
#include <LiquidCrystal.h>


// LCD
LiquidCrystal lcd(35, 33, 31, 29, 27, 25);

// Temperature, humidity sensor
#define DHTPIN 7
DHT11 dht11(DHTPIN);

// ADC
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// Port H - Stepper motor
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 40, 42, 44, 46);
volatile unsigned char* DDRH_REG = (volatile unsigned char*) 0x101;
volatile unsigned char* PORTH_REG = (volatile unsigned char*) 0x102;

// Port E - Start, Stop button Port
volatile unsigned char* DDRE_REG = (volatile unsigned char*) 0x0D;
volatile unsigned char* PORTE_REG = (volatile unsigned char*) 0x0E;

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

// vent flag
volatile bool ventAngleIncreased = 0;

// water level flag
volatile bool waterLevelCritical = 0;

// function prototypes
void disabledState();
void idleState();
void errorState();
void runningState();
void transitionTo(enum states nextState);
void printStatus();
int get_temp_and_humidity(int* temp, int* humidity);
void setFan(bool on);
void setLED(const bool Y, const bool G, const bool R, const bool B);
void ventAngle(bool direction);
void startStopInterrupt();
void printTime();
void U0init(unsigned long U0baud);
void printToSerial(char message[], int size);
void U0putchar(unsigned char U0pdata);

void setup() {

  adc_init();

  lcd.begin(16, 2);

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
  //*DDRE_REG |= (0x0 << 4); // Pin D2 (Port E4) to INPUT mode
  attachInterrupt(digitalPinToInterrupt(2), startStopInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(3), changeVentAngle, RISING);

  // vent buttons - INPUT mode
  *DDRE_REG &= ~(0x1 << 3); // D3

  // Real Time Clock
  if (! rtc.begin()) {
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

}

void loop() {

  int waterLevel = adc_read(0);

  if (waterLevel == 0){

    waterLevelCritical = 1;

  } else if (waterLevel > 0){

    waterLevelCritical = 0;

  }


  int temperature = 0;
  int humidity = 0;
  if (coolerState != DISABLED){

    // Report Humidity, Temperature
    int result = get_temp_and_humidity(&temperature, &humidity);

    // Output to LCD
    if (result == 0) {

      char message1[16];
      char message2[16];
      
      lcd.setCursor(0, 0);
      sprintf(message1, "Temp = %d C", temperature);
      lcd.print(message1);

      lcd.setCursor(0, 1);
      sprintf(message2, "RH = %d %%", humidity);
      lcd.print(message2);
      
    }

  }

  // Handle Vent Angle change
  if (ventAngleIncreased == 1){

    ventAngle(0);
    ventAngleIncreased = 0;

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
      idleState(temperature);
      break;

    case ERROR:
      errorState();
      break;

    case RUNNING:
      runningState(temperature, humidity);
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

void idleState(int temperature){

  // Green LED ON
  setLED(0, 1, 0, 0);

  // Monitor water level
  if (waterLevelCritical) {

    transitionTo(ERROR);

  } else if (temperature > 20) {

    transitionTo(RUNNING);

  }

}

void errorState(){

  // Turn off Fan
  fanOn = false;

  // Display Error to LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ERROR");

  // Red LED ON
  setLED(0, 0, 1, 0);

}

void runningState(int temperature, int humidity){

  // Run Cooler
  fanOn = true;

  // blue LED on
  setLED(0, 0, 0, 1);

  // Check water level
  bool waterLevelCritical = false;

  if (waterLevelCritical) {

    transitionTo(ERROR);

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
  messageLength = snprintf(message, sizeof(message), "Vent Angle Increased: %d", ventAngleIncreased);
  printToSerial(message, messageLength);
  ventAngleIncreased = 0;

}

int get_temp_and_humidity(int* temp, int* humidity) {
  return dht11.readTemperatureHumidity(*temp, *humidity);
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

  myStepper.setSpeed(5);

  if(direction){

    myStepper.step(stepsPerRevolution/4);
    ventAngleIncreased = 1;

  } else if (!direction){

    myStepper.step(-1*stepsPerRevolution/4);
    ventAngleIncreased = 0;

  }

}

void changeVentAngle(){

  ventAngleIncreased = 1;

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

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bitsd
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

