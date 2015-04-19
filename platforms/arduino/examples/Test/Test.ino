/* basic SPI */
#include <SPI.h>

#ifdef ENERGIA

// CMM 9301 Configuration for TI Launchpad
#define PIN_SCK      7
#define PIN_CS       8
#define PIN_SHUTDOWN 11
#define PIN_IRQ_DATA 13
#define PIN_MISO     14
#define PIN_MOSI     15
#else // ARDUINO

// CMM 9301 Configuration on Arduino
#define PIN_IRQ_DATA 2
#define PIN_CS 4
#define PIN_SHUTDOWN 5
#define PIN_MISO 50
#define PIN_MOSI 51
#define PIN_SCK  52
#define PIN_SHUTDOWN
#endif

#if 0
// software SPI
class Software_SPI {
public:
  void setBitOrder(int){}
  void setDataMode(int){};
  void setClockDivider(int){};
  void begin(){
  }
  void end(){
  }
  uint8_t transfer(uint8_t data){
    int i;
    for (i=0;i<8;i++){
      if (data & 0x80){
        digitalWrite(PIN_MOSI, HIGH);
      } else {
        digitalWrite(PIN_MOSI, LOW);
      }
      digitalWrite(PIN_SCK, HIGH);
      data = data << 1;
      digitalWrite(PIN_SCK, LOW);
    }  
    return 0;
  }
};

#define SPI_MODE0 0
#define SPI_CLOCK_DIV8 8

Software_SPI SPI;
#endif

void setup(){
  pinMode (PIN_CS, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_SHUTDOWN, OUTPUT);
  
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  // digitalWrite(PIN_SHUTDOWN, LOW);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_MOSI, LOW);

  Serial.begin(9600);
  Serial.println("Started\n");
}

void send_reset(){
  pinMode(PIN_MOSI, OUTPUT);
  digitalWrite(PIN_MOSI, HIGH);
  delay(1);
  digitalWrite(PIN_CS, LOW);
  delay(1);
  SPI.begin(); 
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  SPI.transfer(0x01);
  SPI.transfer(0x03);
  SPI.transfer(0x0c);
  SPI.transfer(0x00);
  SPI.end(); 
  pinMode(PIN_MOSI, OUTPUT);
  digitalWrite(PIN_MOSI, LOW);
  digitalWrite(PIN_CS, HIGH);
}

void send_illegal(){
  digitalWrite(PIN_MOSI, HIGH);
  pinMode(PIN_MOSI, OUTPUT);
  delay(1);
  digitalWrite(PIN_CS, LOW);
  delay(1);
  SPI.begin(); 
  int i;
  for (i=0;i<255;i++){
    SPI.transfer(0xff);
  }    
  SPI.end(); 
  digitalWrite(PIN_CS, HIGH);
}

void flush_input(){
  pinMode(PIN_MOSI, OUTPUT);
  digitalWrite(PIN_MOSI, LOW);
  digitalWrite(PIN_CS, LOW);
  SPI.begin(); 
  while (digitalRead(PIN_IRQ_DATA) == HIGH){
    SPI.transfer(0x00);
  }
  SPI.end(); 
  digitalWrite(PIN_CS, HIGH);
}

void read_event(){
  do {
    pinMode (PIN_CS, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    digitalWrite(PIN_MOSI, LOW);
    delay(1);
    digitalWrite(PIN_CS, LOW);
    delay(1);
    SPI.begin(); 
    uint8_t data = SPI.transfer(0x00);
    Serial.print("Read 0x");
    Serial.println(data, HEX);
    SPI.end(); 
    pinMode (PIN_CS, OUTPUT);
    pinMode(PIN_MOSI, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    delay(1);
  } while (digitalRead(PIN_IRQ_DATA) == HIGH);
}
 
void send_noise(){
  while (1){
    Serial.print(".");
    pinMode(PIN_MOSI, OUTPUT);
    digitalWrite(PIN_MOSI, HIGH);
    SPI.begin(); 
    digitalWrite(PIN_CS, LOW);
    SPI.transfer(0x0f);
    SPI.transfer(0x55);
    SPI.transfer(0xf0);
    digitalWrite(PIN_CS, HIGH);
    SPI.end();
    pinMode(PIN_MOSI, OUTPUT);
    digitalWrite(PIN_MOSI, LOW);
  } 
  Serial.println("\n");  
}

void power_cycle(){
  // power cycle
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_CS, INPUT);
  digitalWrite(PIN_SHUTDOWN, HIGH);
  delay(1000);
  digitalWrite(PIN_SHUTDOWN, LOW);
  delay(1000);
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
}

void loop() {

  Serial.println("Send noise");
  
  // prepare unsynced state
  // send_noise();

  // bring HCI parser into defined error state 
  // send_illegal();
  
  // power cycle
  Serial.println("Power cycle");
  power_cycle();
   
  Serial.println("Reset");
  send_reset();

  while (digitalRead(PIN_IRQ_DATA) == HIGH){
    read_event();
  }
   
  delay(5000);
}
