#include <actions.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
// TODO: Update primary repo with pin updates (maybe not necessary)
#define I2C_PORT i2c0
#define REFRESH_RATE 33 // 60 Hz Refres rate 
const uint LED_PIN = PICO_DEFAULT_LED_PIN;

//const uint POT_MIN = 0xB0; // Represents the PONG bar at the bottom of screen
//const uint POT_MAX = 0xEE; // Represents the PONG bar at the top of screen

// New bounds based on using Pico 5V rail - behaves differently than using ATARI supply
const uint POT_MIN = 0x0A; // Represents the PONG bar at the bottom of screen
const uint POT_MAX = 0x2b; // Represents the PONG bar at the top of screen
// ^^^^^^^ Adjusted above bounds for dead zones

const uint POT_MAX_OHM = 1000000;
const uint I2C_SCL_POT = 1;
const uint I2C_SDA_POT = 0;
static uint addr = 0x2C; // Read datasheet of AD5241 device for addr specifics
static uint instr = 0x80; /* Instruction byte to adjust RDAC2 (Digital POT 2) on the AD5242 device 
                            Midscale reset is turned off, and the shutdown feature is off
                            Output logic pins on chip are both set to 0              */

uint8_t paddle_pos = (POT_MAX + POT_MIN) / 2; // Global variable for tracking paddle position between loop_use_actions() executions
double ohms = 0; // Global variable to track ohm reading

const uint8_t step_size = 4;
const uint8_t jitter_size = 2; 

/* FULL Output Mapping (Network Simulation to Atari Output
 *
 * KEY:
[ SIMULATED ACTION    ] -> [ ALE MAPPING    ] -> [ ENCODER MAPPING  ] -> [ POT CHANGE ] -> [ OHM Change ] -> [ ATARI OUTPUT ]

[ DONT MOVE ON SCREEN ] -> [ NOOP       - 0 ] -> [NETWORK ACTION - 0] -> [ +0         ] -> [   NONE     ] -> [ STAY         ]
[ MOVE UP ON SCREEN   ] -> [ MOVE RIGHT - 3 ] -> [NETWORK ACTION - 1] -> [ +STEP      ] -> [ - OHM      ] -> [ UP           ]
[ MOVE DOWN ON SCREEN ] -> [ MOVE LEFT  - 4 ] -> [NETWORK ACTION - 2] -> [ -STEP      ] -> [ + OHM      ] -> [ DOWN         ]

(Top and Bottom are related to if pong paddle is on Top or Bottom of the screen
POT RANGE - 
Top:    0x EF
Bottom: 0x AC
Range : 67 ticks

Resistance OHM -
Top:    72 k OHM
Bottom: 312 k OHM
*/

/*
 * Jitters pong paddle on screen slightly
 *   - Useful for helping the event-based camera continue to see the paddle
 *   even if it is stuck on the top or bottom of the screen
 */
void jitter(uint8_t val); // Jitters pong paddle on screen slightly 


/*
 * Jitters pong paddle on screen slightly
 *   - Useful for helping the event-based camera continue to see the paddle
 *   even if it is stuck on the top or bottom of the screen
 */
void jitter(uint8_t val); // Jitters pong paddle on screen slightly 

uint8_t update_paddle(double actions[], uint8_t num_actions, uint8_t val, uint8_t step_size){
  uint new_val = val;

  //printf("num_actions = %u\n", num_actions); 
  for (uint8_t i = 0; i < num_actions; ++i) {
    switch ( (int) actions[i] ) {
    /* presumably NOOP */
    case 0:
      break;
	  
    /* presumably MOVE_RIGHT */
    case 1:
      printf("action %u: MOVE_RIGHT\n", i);
      new_val += step_size;
      break;

    /* presumably MOVE_LEFT */
    case 2:
      printf("action %u: MOVE_LEFT\n", i);
      new_val -= step_size;
      break;
    
    default:
      printf("error in update_paddle: action %d not recognized\n", (int) actions[0]);
      exit(1);
    }
  }
  // Check boundary conditions (TODO: remove if the SNN isn't moving to where it wants to go)
  if (new_val > POT_MAX) return POT_MAX;
  if (new_val < POT_MIN) return POT_MIN;
  return new_val;
}

void update_pot(uint8_t val){ // These check conditions might be dumb. Might just overflow to save number
  int ret = 0; 
  if (val < 0 || val > 255){
    //printf("Invalid POT value. Exiting Program\n");
    gpio_put(LED_PIN, 0);
    exit(-1);
  }
  uint8_t buf[2] = {instr,val}; 
  ret = i2c_write_blocking(I2C_PORT, addr, buf, 2, false); 
  if (ret == PICO_ERROR_GENERIC) printf("Error on I2C Bus\n");
  return;

}

void jitter(uint8_t val){
  
  uint8_t right = val + jitter_size;
  uint8_t left = val - jitter_size;
  // Ensures that maximum bounds are not exceeded (causes hard fault) 
  if (right > POT_MAX) right = POT_MAX;
  if (left < POT_MIN) left =  POT_MIN;
  
  // Sleeps in between each position update to give Atari time to react 
  update_pot(right);
  sleep_ms(33);
  update_pot(left);
  sleep_ms(33);
  update_pot(val);

  return;
}


uint8_t read_pot(){
  uint8_t output; 
  i2c_read_blocking(I2C_PORT, addr, &output, 1, false);
  //printf("POT Value: %x\n",output);
  return output; 
}

/* User defined: Use this procedure to perform any setup needed to utilize the
action values retrieved from the output of the neuroprocessor's network. Called
once.
    Arguments:
        - available_pins : Pins available for user to use as desired
        - num_available_pins : Number of pins available for user use
*/
void setup_use_actions(const uint8_t available_pins[], uint8_t num_available_pins) {
  // Setup output LED PIN    
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN,0); 

  /* Setup I2C Protocol for I2C0 @ 400kHz 
  with GPIO 1 (SDL) and GPIO 0 (SDA)*/
  i2c_init(I2C_PORT, 400000);
  gpio_set_function(I2C_SCL_POT, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SDA_POT, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SCL_POT);
  gpio_pull_up(I2C_SDA_POT);

  return;
}

/* User defined: Use this procedure to utilize action values retrieved from the
output of the neuroprocessor's network. Called repeatedly.
    Arguments:
        - actions : Array of double action values
        - num_actions : Number of action values in actions array
        - available_pins : Pins available for user to use as desired
        - num_available_pins : Number of pins available for user use
*/
void loop_use_actions(double actions[], uint8_t num_actions, const uint8_t available_pins[], uint8_t num_available_pins) {
  double ohms;

  /*
  if (num_actions > 1){
    printf("ERROR: Wrong number of actions (expecting only 1)"); 
    return; 
  }
  */
  //printf("received action %d\n", (int) actions[0]);

  paddle_pos = update_paddle(actions, num_actions, paddle_pos, step_size); 
  ohms = ((double) paddle_pos / 256) * POT_MAX;
 
  //printf("Setting POT to %x ( %3.2f ohms)\n", paddle_pos, ohms);
  update_pot(paddle_pos);

  // Heartbeat
  gpio_put(LED_PIN, !gpio_get(LED_PIN));    

  //sleep_ms(REFRESH_RATE); // Might be uneccessary due to SNN timing TODO: see if this needs to be removed
  read_pot();      
}
