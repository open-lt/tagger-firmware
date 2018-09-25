#include "BluetoothSerial.h"
#include <pthread.h>

#define DATA_BYTES_POS 2 //0 command byte, 1 address byte, 2 data bytes
#define DATA_BYTES_LENGHT 4 //sizeof unsinged long
#define BT_MESSAGE_LENGHT 6
#define COMMAND_WRITE 0x01
#define COMMAND_STREAM 0x02
#define COMMAND_READ 0x03 //TODO not used yet
#define ADDR_SEND_BYTES  0x01
#define ADDR_SHOOT_COOLDOWN 0x02
#define ADDR_SHOOT_DELAY 0x03
#define ADDR_SHOOT_DURATION_MIN 0x04
#define ADDR_SHOOT_DURATION_MAX 0x05
#define ADDR_SHOOT_MODE 0x06
#define ADDR_BURST_AMOUNT_MIN 0x07
#define ADDR_BURST_AMOUNT_MAX 0x08
#define ADDR_BURST_COOLDOWN 0x09
#define ADDR_MAGAZINE_SIZE 0x0A
#define PIN_TRIGGER 5
//#define PIN_RELOAD
#define SEND_TRIGGER_STATUS_INTERVAL 100   //use this or when status change? not used
#define CHECK_INTERVAL_BT_IN_MS 100 //not used
#define CHECK_INTERVAL_IR_IN_MS 100 //not used
#define SHOOT_MODE_MANUAL 0x00000001
#define SHOOT_MODE_AUTO 0x00000002

//struct for configuration of ir behaviour when in autonomous mode
typedef struct Shoot_config {

  //unsigned long = 4 bytes
  unsigned long send_bytes = 0x00FFFFFF, //standard value without bt connection
                cooldown_in_ms = 1000, //min 1000 ms break after end of shot
                trigger_delay_in_ms = 0,  //0 ms delay after triggershoot
                duration_min_in_ms = 100, //shot duration 100 ms
                duration_max_in_ms = 100,
                smode = SHOOT_MODE_MANUAL,
                burst_amount_min = 1, //one shot per trigger
                burst_amount_max = 1,
                burst_cooldown_in_ms = 0,  //no burst shot
                magazine_size = 0, //infinte
                current_magazine_level = 0;
} Shoot_config;

Shoot_config    shootconf;
volatile bool   trigger_status=false;
unsigned long   time_in_ms = millis(); 
pthread_t       thread_send_bt;
pthread_t       thread_send_ir;
pthread_t       thread_handle_bt;

typedef enum    {AUTONOMOUS, STREAM}    state_t;
state_t         state = AUTONOMOUS;

HardwareSerial  ir(1); // rx_pin=3, tx_pin=1
//HardwareSerial audio(2); //rx_pin=9, tx_pin=10
//HardwareSerial serial(3); //rx_pin=16, tx_pin=17
BluetoothSerial bt;




/*    >>>>>    SETUP    <<<<<<     */




void setup() {
  uint8_t set_ir_baudrate_9600[5]={0xA1, 0xF3, 0x02, 0x00, 0x00};
  int i=0; // i is here for no reason, I didn't make it compile without  

  ir.begin(9600);
  bt.begin("ESP32");
  pinMode(PIN_TRIGGER, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), handle_trigger, CHANGE); 
  ir.write(set_ir_baudrate_9600,sizeof(set_ir_baudrate_9600));

  //create threads
  pthread_create(&thread_send_bt, NULL, send_bt, (void *)i);// i is here for no reason, I didn't make it compile without  
  pthread_create(&thread_send_ir, NULL, send_ir, (void *)i );// i is here for no reason, I didn't make it compile without  
  pthread_create(&thread_handle_bt, NULL, handle_bt, (void *)i );// i is here for no reason, I didn't make it compile without  
}




/*    >>>>>    LOOP    <<<<<<     */




//very short loop function, is this a good style? I don't know
void loop() {
  time_in_ms = millis();
}





/*    >>>>>    FUNCTIONS    <<<<<<     */




void handle_trigger() {
  trigger_status = !trigger_status;
  return;
}

//TODO: magazine size
void *send_ir(void *i) {

  unsigned long shoot_timestamp = 0,
                burst_counter=0;
  typedef enum {READY, DELAY, SHOOTING, COOLDOWN, BURST_COOLDOWN} shoot_status;
  shoot_status shoot_phase;

  while(true) {
    switch(state) {
      case STREAM:
        // do nothing
        break;
        
      case AUTONOMOUS:
  
        switch(shoot_phase) {
  
          //ready to receive trigger signal
          case READY:
            if (trigger_status) {
              shoot_timestamp = time_in_ms;
              shoot_phase = DELAY;
            }
            break;
  
          //do nothing until delay is over
          case DELAY:
            if (time_in_ms - shoot_timestamp >= shootconf.trigger_delay_in_ms) {
              shoot_timestamp = time_in_ms;
              shoot_phase = SHOOTING;
            }
            break;
  
          //send ir, duration depending on trigger status
          case SHOOTING:
          
            //trigger pressed: long shot / long burst
            if (trigger_status) { 
              if (time_in_ms - shoot_timestamp >= shootconf.duration_max_in_ms) {
                shoot_timestamp = time_in_ms;
                shoot_phase = COOLDOWN;
                burst_counter++;

                if (shootconf.burst_amount_max >= burst_counter) shoot_phase = BURST_COOLDOWN;
                else shoot_phase = COOLDOWN;
              }            
              //TODO disable interrupt to not disturb ir?
              //else //TODO ir send shootconf->shoot bytes
            }
            
            //trigger released: short shot / short burst
            else { 
              if (time_in_ms - shoot_timestamp >= shootconf.duration_min_in_ms) {
                shoot_timestamp = time_in_ms;
                shoot_phase = COOLDOWN;
                burst_counter++;

                if (shootconf.burst_amount_min >= burst_counter) shoot_phase = BURST_COOLDOWN;
                else shoot_phase = COOLDOWN;
              }
              //TODO disable interrupt to not disturb ir?
              //else //TODO ir send shootconf->shoot bytes
            }
            break;
  
          //short cooldown, back to shooting
          case BURST_COOLDOWN:
            if (time_in_ms - shoot_timestamp >= shootconf.cooldown_in_ms) {
              shoot_timestamp = time_in_ms;
              shoot_phase = SHOOTING;
            }
            
            break;
  
          //befor getting ready, wait for cooldown, depending on mode
          case COOLDOWN:
            burst_counter = 0;
            
            if (time_in_ms - shoot_timestamp >= shootconf.cooldown_in_ms) {
              
              //leave cooldown in manual mode only when trigger released
              if (shootconf.smode == SHOOT_MODE_MANUAL && !trigger_status) shoot_phase = READY;
              else if (shootconf.smode == SHOOT_MODE_AUTO) shoot_phase = READY;
              }
          break;
          
        }
      break;
    }
  }
}


void *send_bt(void *i) { // i is here for no reason, I didn't make it compile without  
  int ir_data = 0;   // for incoming serial data from ir
  bool trigger_old_status = false;

  while(true) {
    //TODO?: use time interval?
    if (trigger_status =! trigger_old_status) {
        bt.println("Trigger status: ");
        bt.print(trigger_status);
        trigger_old_status = trigger_status;
      }
    else {
      while (ir.available() > 0) {
        ir_data = ir.read();
        bt.println("Incoming IR:");
        bt.print(ir_data); //TODO: is this correct?
      }
    }
  }
}

void stream_bt_to_ir() {
  unsigned char bt_data = 0; // the data given from smart phone app  
   
  while (true) {
    while (bt.available() > 0) {
      bt_data=bt.read();
      if (bt_data='\n') {
        state = AUTONOMOUS;
        return;
      }
      //else ir.write(bt_data,sizeof(bt_data)); //TODO bt_data data type conversion?
    }
  }
}

void bt_to_config() {

  unsigned char bt_data[BT_MESSAGE_LENGHT];
  uint8_t byte_counter=0; 
  
  while(true) {

    if (bt.available() > 0) {
      bt_data[byte_counter]=bt.read();
      byte_counter++;
      
      //when command byte == COMMAND_STREAM go directly to stream mode
      if(bt_data[0] == COMMAND_STREAM) {
        state = STREAM;
        return;
      }
      
      //receive BT_MESSAGE_LENGHT bytes then handle message
      else if(byte_counter >= BT_MESSAGE_LENGHT) {
        //check command byte
        if(bt_data[0] == COMMAND_WRITE) {
      
          //check address byte and write to conf
          if(bt_data[1] == ADDR_SEND_BYTES) write_to_config(bt_data, &shootconf.send_bytes);
          else if(bt_data[1] == ADDR_SHOOT_COOLDOWN) write_to_config(bt_data, &shootconf.cooldown_in_ms);
          else if(bt_data[1] == ADDR_SHOOT_DELAY) write_to_config(bt_data, &shootconf.trigger_delay_in_ms);
          else if(bt_data[1] == ADDR_SHOOT_DURATION_MIN) write_to_config(bt_data, &shootconf.duration_min_in_ms);
          else if(bt_data[1] == ADDR_SHOOT_DURATION_MAX) write_to_config(bt_data, &shootconf.duration_max_in_ms);
          else if(bt_data[1] == ADDR_SHOOT_MODE) write_to_config(bt_data, &shootconf.smode);
          else if(bt_data[1] == ADDR_BURST_AMOUNT_MIN) write_to_config(bt_data, &shootconf.burst_amount_min);
          else if(bt_data[1] == ADDR_BURST_AMOUNT_MAX) write_to_config(bt_data, &shootconf.burst_amount_max);
          else if(bt_data[1] == ADDR_BURST_COOLDOWN) write_to_config(bt_data, &shootconf.burst_cooldown_in_ms);
          else if(bt_data[1] == ADDR_MAGAZINE_SIZE) write_to_config(bt_data, &shootconf.magazine_size);        
      
          //TODO else error when unknown address
        }
      
        else if (bt_data[0] == COMMAND_READ) {
            //TODO send config via bt
        
          //error when unknown command byte
          // TODO else error
          memset(bt_data,0,BT_MESSAGE_LENGHT*sizeof(unsigned char));
          byte_counter = 0;
        }
      }
    }
  }
}

void write_to_config(unsigned char bt_data[BT_MESSAGE_LENGHT], unsigned long *conf) {

  *conf = 0;
  
  for (int i=0; i<DATA_BYTES_LENGHT-1; i++) {
      shootconf.send_bytes <<= sizeof(unsigned char);
      *conf |= (unsigned long)(bt_data[DATA_BYTES_POS+i]);
  }
}

void *handle_bt(void *i) {
  while(true) {
    switch(state) {
      case AUTONOMOUS:
        bt_to_config();
        break;
      case STREAM:
        stream_bt_to_ir();
        break;
    }
  }
}
