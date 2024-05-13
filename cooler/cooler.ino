#include <RTClib.h>
#include <Stepper.h>
#include <DHT11.h>
#include <LiquidCrystal.h>

#define RDA 0x80
#define TBE 0x20  

// Define ADC Registers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

// Define UART Registers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// Define Port E Register Pointers (FAN)
volatile unsigned char* port_e = (unsigned char*) 0x2E; 
volatile unsigned char* ddr_e  = (unsigned char*) 0x2D; 
volatile unsigned char* pin_e  = (unsigned char*) 0x2C;

// Define Port A Register Pointers (LEDs)
volatile unsigned char* port_a = (unsigned char*) 0x22; 
volatile unsigned char* ddr_a  = (unsigned char*) 0x21; 
volatile unsigned char* pin_a  = (unsigned char*) 0x20;  

// Define Port L Register Pointers (Stepper Motor, Vent Control, Reset)
const int stepsPerRevolution = 2000;
Stepper myStepper = Stepper(stepsPerRevolution, 46, 47, 48, 49);
volatile unsigned char* port_l = (unsigned char*) 0x10B;
volatile unsigned char* ddr_l = (unsigned char*) 0x10A;
volatile unsigned char* pin_l = (unsigned char*) 0x109;

// Cooler States
enum States : byte {NOTHING, DISABLED, IDLE, ERROR, RUNNING} C_STATE, P_STATE;

// LCD
LiquidCrystal lcd(33, 31, 29, 27, 25, 23);

// Realtime Clock
RTC_DS1307 rtc;

// DHT11 Sensor
DHT11 dht11(7);
volatile int TEMP;
volatile int HUMID;

// DELAY
const int PERIOD = 60000;
volatile unsigned long time_now = 0;

// Vent Angle
volatile int VENT_ANGLE = 0;

// Thresholds
const int W_THRESHOLD = 50;
const int T_THRESHOLD = 10;

void setup() {
  
  // FAN MOTOR
  set_ddr(ddr_e, 3, OUTPUT);
  set_ddr(ddr_e, 5, OUTPUT);
  
  // LEDS
  set_ddr(ddr_a, 0, OUTPUT);
  set_ddr(ddr_a, 2, OUTPUT);
  set_ddr(ddr_a, 4, OUTPUT);
  set_ddr(ddr_a, 6, OUTPUT);

  // VENT
  set_ddr(ddr_l, 4, INPUT);
  set_ddr(ddr_l, 5, INPUT);

  // RESET
  set_ddr(ddr_l, 7, INPUT);
  
  // START/STOP
  attachInterrupt(digitalPinToInterrupt(2), toggle, RISING);

  // DISPLAY/CONSOLE
  adc_init();
  U0init(9600);
  lcd.begin(16, 2);

  // INITIAL STATES
  C_STATE = DISABLED;
  P_STATE = NOTHING;

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
  switch (C_STATE) {
    case DISABLED:
      disabled();
      break;
    case IDLE:
      idle();
      break;
    case ERROR:
      error();
      break;
    case RUNNING:
      running();
      break;
  }
}

//GPIO
void set_ddr(unsigned char* ddr, unsigned char pin_num, unsigned char direction) {
  if (direction == INPUT)
    *ddr &= ~(0x01 << pin_num);
  else if (direction == OUTPUT)
    *ddr |= 0x01 << pin_num;
}

void write_port(unsigned char* port, unsigned char pin_num, unsigned char state) {
  if(state == LOW)
    *port &= ~(0x01 << pin_num);
  else if (state == HIGH)
    *port |= 0x01 << pin_num;
}

unsigned char read_pin(unsigned char* pin, unsigned char pin_num) {
  return (*pin & (0x01 << pin_num)) > LOW;
}

//States Logic
void disabled() {
  if (P_STATE != C_STATE) {
    P_STATE = C_STATE;
    lcd.clear();

    // Solo Yellow LED
    write_port(port_a, 0, HIGH);
    write_port(port_a, 2, LOW);
    write_port(port_a, 4, LOW);
    write_port(port_a, 6, LOW);

    // FAN
    write_port(port_e, 3, LOW);
    write_port(port_e, 5, LOW);

    status();
  }

  adjust_vent();
}

void idle() {
  if (P_STATE != C_STATE) {
    P_STATE = C_STATE;
    lcd.clear();  

    // Solo Green LED
    write_port(port_a, 0, LOW);
    write_port(port_a, 2, HIGH);
    write_port(port_a, 4, LOW);
    write_port(port_a, 6, LOW);

    // FAN
    write_port(port_e, 3, LOW);
    write_port(port_e, 5, LOW);

    status();
  }

  adjust_vent();

  get_temp_and_humidity(&TEMP, &HUMID);
  
  // Display Temp and Humidity
  lcd.setCursor(0, 0);
  char buf[17];
  snprintf(buf, 17, "TEMP: %d C", TEMP);
  lcd.print(buf);

  lcd.setCursor(0, 1);
  snprintf(buf, 17, "RH: %d %%", HUMID);
  lcd.print(buf);

  // Transition to RUNNING if hot enough
  if (TEMP > T_THRESHOLD)
    C_STATE = RUNNING;

  // Transition to ERROR if not enough water
  if (adc_read(0) > W_THRESHOLD) return;

  C_STATE = ERROR;
}

void error() {
  if (P_STATE != C_STATE) {
    P_STATE = C_STATE;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WATER LOW");

    // Solo Red LED
    write_port(port_a, 0, LOW);
    write_port(port_a, 2, LOW);
    write_port(port_a, 4, HIGH);
    write_port(port_a, 6, LOW);

    // FAN
    write_port(port_e, 3, LOW);
    write_port(port_e, 5, LOW);

    status();
  }

  reset();
}

void running() {
  if (P_STATE != C_STATE) {
    P_STATE = C_STATE;
    lcd.clear();

    // Solo Blue LED
    write_port(port_a, 0, LOW);
    write_port(port_a, 2, LOW);
    write_port(port_a, 4, LOW);
    write_port(port_a, 6, HIGH);

    // FAN
    write_port(port_e, 3, HIGH);
    write_port(port_e, 5, HIGH);

    status();
  }

  adjust_vent();

  get_temp_and_humidity(&TEMP, &HUMID);

  // Display Temp and Humidity
  lcd.setCursor(0, 0);
  char buf[17];
  snprintf(buf, 17, "TEMP: %d C", TEMP);
  lcd.print(buf);

  lcd.setCursor(0, 1);
  snprintf(buf, 17, "RH: %d %%", HUMID);
  lcd.print(buf);

  // Transition to RUNNING if hot enough
  if (TEMP <= T_THRESHOLD)
    C_STATE = IDLE;

  // Transition to ERROR if not enough water
  if (adc_read(0) > W_THRESHOLD) return;

  C_STATE = ERROR;
}

// Vent Control
void adjust_vent() {
  myStepper.setSpeed(10);

  // Rotate clockwise, otherwise counter
  if (read_pin(pin_l, 4) && !(read_pin(pin_l, 5))) {
    myStepper.step(stepsPerRevolution/4);
    VENT_ANGLE = (VENT_ANGLE + 90) % 360;
    status();
  }
  else if (!(read_pin(pin_l, 4)) && read_pin(pin_l, 5)) {
    myStepper.step(-stepsPerRevolution/4);
    VENT_ANGLE = (VENT_ANGLE - 90) % 360;
    status();
  }
}

//Monitoring
void status() {
  print_state();
  print_time();
  print_angle();
}

void print_state() {
  print_string("State: ");
  switch (C_STATE) {
    case DISABLED:
      print_stringln("DISABLED");
      break;
    case IDLE:
      print_stringln("IDLE");
      break;
    case ERROR:
      print_stringln("ERROR");
      break;
    case RUNNING:
      print_stringln("RUNNING");
      break;
  }
}

void print_time() {
  DateTime now = rtc.now();
  char buf[20];

  snprintf(buf, 20, "Time: %d:%d:%d", now.hour(), now.minute(), now.second());
  print_stringln(buf);
}

void print_angle() {
  char buf[20];
  snprintf(buf, 20, "Vent Angle: %d deg", VENT_ANGLE);
  print_stringln(buf);
}

void get_temp_and_humidity(int* temp, int* humidity) {
  // Delay for minute
  if (millis() >= time_now + PERIOD) {
    time_now += PERIOD;
    dht11.readTemperatureHumidity(*temp, *humidity);
  }
}

// Reset
void reset() {
  // Allow reset if enough water
  if (adc_read(0) > W_THRESHOLD && read_pin(pin_l, 7)) {
    C_STATE = IDLE;
  }
}

//ISR
void toggle() {
  C_STATE = (C_STATE != DISABLED) ? DISABLED : IDLE;
}

//UART
void U0init(unsigned long U0baud) {
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return (*myUCSR0A & RDA);
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while(!(*myUCSR0A & TBE));
  *myUDR0 = U0pdata;
}

void print_string(char message[]) {
  int i = 0;
  while (message[i] != '\0'){
    U0putchar((unsigned char) message[i++]);
  }
}

void print_stringln(char message[]) {
  print_string(message);
  U0putchar('\n');
}

//ADC
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
