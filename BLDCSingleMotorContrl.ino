/*
  Synchronized three phase bldc motor to hsync or vsync signal
  for beam rasterizing vga video signal
*/

//pins 6-11 used for internal flash memory
//    GPIO16: pin is high at BOOT
//    GPIO0:                       boot failure if pulled LOW
//    GPIO2:  pin is high on BOOT, boot failure if pulled LOW
//    GPIO15:                      boot failure if pulled HIGH
//    GPIO3:  pin is high at BOOT
//    GPIO1:  pin is high at BOOT, boot failure if pulled LOW
//    GPIO10: pin is high at BOOT
//    GPIO9:  pin is high at BOOT

#define SYNC_PULSE           3  //allow to go high at boot
#define BOOST_VOLTAGE_SENSE  2  //allow to go high at boot

#define MOT_GATE_BOOST       5  //neutral at boot

#define MOT_A_HIGH           15 //low at boot
#define MOT_A_LOW            4  //neutral at boot
#define MOT_B_HIGH           14
#define MOT_B_LOW            12
#define MOT_C_HIGH           13
#define MOT_C_LOW            16 //high at boot

bool AHigh[6] = { 0, 0, 0, 0, 1, 1 };
bool ALow [6] = { 0, 1, 1, 0, 0, 0 };

bool BHigh[6] = { 1, 1, 0, 0, 0, 0 };
bool BLow [6] = { 0, 0, 0, 1, 1, 0 };

bool CHigh[6] = { 0, 0, 1, 1, 0, 0 };
bool CLow [6] = { 1, 0, 0, 0, 0, 1 };

int offst  = 0;
int loopct = 0;

// the setup function runs once when reset is pressed or the board powers on
void setup() {

  Serial.begin(115200);//230400);
  delay(100);
  for(int i =0; i < 10; ++i){
    Serial.printf("Serial begin, outputting to synchronize %d \n", i);
  }

  //initalize input pins
  pinMode(          SYNC_PULSE, INPUT  ); //digitalWrite(          SYNC_PULSE   , 0    );
  delay(100);
  Serial.println("sync pulse input initalized");
  pinMode( BOOST_VOLTAGE_SENSE, INPUT_PULLUP ); //digitalWrite( BOOST_VOLTAGE_SENSE   , 0    );
  delay(100);
  Serial.println("BOOST_VOLTAGE_SENSE input initalized");
  
  //initialize the output pins (keep output mosfets switched off until gate drive voltage has been pumped to threshold
  //to avoid h bridge +- shoot through and burning out the mosfets)
  pinMode( MOT_A_HIGH     , OUTPUT ); digitalWrite( MOT_A_HIGH    , LOW  ); //high pulls up the gate drive voltage
  delay(100);
  Serial.println("mot a h init");
  pinMode( MOT_A_LOW      , OUTPUT ); digitalWrite( MOT_A_LOW     , LOW  );
  delay(100);
  Serial.println("mot a l init");
  pinMode( MOT_B_HIGH     , OUTPUT ); digitalWrite( MOT_B_HIGH    , LOW  );
  delay(100);
  Serial.println("mot b h init");
  pinMode( MOT_B_LOW      , OUTPUT ); digitalWrite( MOT_B_LOW     , LOW  );
  delay(100);
  Serial.println("mot b l init");
  pinMode( MOT_C_HIGH     , OUTPUT ); digitalWrite( MOT_C_HIGH    , LOW  );
  delay(100);
  Serial.println("mot c h init");
  pinMode( MOT_C_LOW      , OUTPUT ); digitalWrite( MOT_C_LOW     , LOW  );
  delay(100);
  Serial.println("mot c l init");
  delay(100);
  Serial.println("motor output initalized");
  delay(100);
  pinMode(    MOT_GATE_BOOST, OUTPUT ); digitalWrite(    MOT_GATE_BOOST, LOW );
  delay(100);
  Serial.println("init complete");
  delay(100);
  
}

int startLoopTime;

unsigned long lastVSyncSenseTime = 0;
unsigned long lastHSyncSenseTime = 0;

//lookup the output phase values and toggle / switch the 6 output mosfets of the 3 Phase H Bridge
inline void setOutputs( uint8 ctr ){
  digitalWrite( MOT_A_HIGH, AHigh[ctr] );
  digitalWrite( MOT_A_LOW , ALow [ctr] );
  
  digitalWrite( MOT_B_HIGH, BHigh[ctr] );
  digitalWrite( MOT_B_LOW , BLow [ctr] );

  digitalWrite( MOT_C_HIGH, CHigh[ctr] );
  digitalWrite( MOT_C_LOW , CLow [ctr] );
}

inline void TurnOffHBridgeOutputMosfets( ){
  //enable micro controller output transitors to pull down gate voltages of output mosfets
  digitalWrite( MOT_A_HIGH    , LOW  );
  digitalWrite( MOT_A_LOW     , LOW  );
  digitalWrite( MOT_B_HIGH    , LOW  );
  digitalWrite( MOT_B_LOW     , LOW  );
  digitalWrite( MOT_C_HIGH    , LOW  );
  digitalWrite( MOT_C_LOW     , LOW  );
}

uint8 ctr = 0; //index in the phase tables

uint8 avgLen = 3;

uint16 lastValueBeforeOutputSwitch = 0;
uint16 offset = 0;

bool MotGateToggle = 0;

bool boostVoltage = 0;

uint8_t recoverCount = 1; //if bridge drive voltage too low recovery takes too long disable outputs

void loop() {
  //Serial.println("loop start");
  delay(2); //prevent 100% duty cycle of the inductor (it gets hot)
  
  boostVoltage = digitalRead( BOOST_VOLTAGE_SENSE );
  //Serial.print("boostVoltage " ); Serial.println( boostVoltage );
  //if the h bridge mosfet driving voltage is lower than specified voltage toggle the boost inductor charging transistor
  if( boostVoltage ){
    digitalWrite( MOT_GATE_BOOST, HIGH ); //energize the inductor until set low a fixed number of cycles/instructions later below
    if(recoverCount != 1){
      if(recoverCount == 0)
         recoverCount = 3000; //give 30 cycles until disabling the outputs
      else
         recoverCount -= 1;
    }
  }else{
   recoverCount = 0;
  }

  //startLoopTime = micros();

  //set the outputs

  //Serial.println("loop before setOutputs");

 // if( recoverCount == 1 ){
    //Serial.print( "off" );
  //  TurnOffHBridgeOutputMosfets(); //make sure outputs are off until enough gate drive voltage has built up
    //delay(1);
  //}else{
    setOutputs( ctr );
  //}
 

  ctr += 1;
  if( ctr >= 6)
    ctr = 0;

    //Serial.println("ctr incremented");
    
  delay(1);
  digitalWrite( MOT_GATE_BOOST, LOW ); //open/break the connection to ground causing the inductor current to flow through the diode
  //to boost the gate drive voltage
  //(also keep the boost transitor off when not pumping to avoid wasting energy/heating/burning out)

  //read the sync pulse for setting the rotation speed
  if( digitalRead( SYNC_PULSE ) == LOW ){
    unsigned long senseTime = micros();
    uint16 vSyncPeriod = senseTime - lastVSyncSenseTime;
    lastVSyncSenseTime = senseTime;
    Serial.printf( "vs\n" );
  }


  //Serial.println( "after sync pulse read" );

  //int loopMillisDiff = micros() - startLoopTime;
  //Serial.printf( "loopMillisDiff %d \n", loopMillisDiff );  //approx 741 microseconds (1micro second = 0.000001 seconds) giving a loop speed of 14705.8824 hz (14khz)
}
