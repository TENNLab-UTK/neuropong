/*
Handling input to spike encoder for binary to decimal converter:
    - Outputs list of binary digits on every iteration
        - Defaults to output of zero, until non-zero signal detected for 3 loops
        - DOES NOT TRACK COLLECTIVE CONSISTENCY OF VALUES, JUST INDIVIDUAL
    - Highest to lowest base goes from lowest button to highest
        - Current implementation: BUTTON_1 = 4s place, BUTTON_2 = 2s place, BUTTON_3 = 1s place
*/

#include <observations.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Define SPI pins
#define SPI_PORT spi0
#define SPI_RDY_PIN 1 // Active high PIN (ready for transfer when  PIN == 3.3V)
#define SPI_SCK_PIN 2
#define SPI_TX_PIN 3
#define SPI_RX_PIN 4
#define SPI_CS_PIN 5

const uint LED_PIN = PICO_DEFAULT_LED_PIN;

#define  OBS_ARR_LENGTH 160
uint8_t observation_arr[OBS_ARR_LENGTH];


void get_observations(){
  int ret = 0;
  uint8_t tmp_rec_array[OBS_ARR_LENGTH];
  //uint8_t tmp_send_array[OBS_ARR_LENGTH] = {0xA};
  // Turn sync pin low to show ready state
  gpio_put(SPI_RDY_PIN, 1);

  // Write dummy bits to SPI and get data from SPI interface
  for(unsigned int i = 0; i < OBS_ARR_LENGTH; i++){

    spi_read_blocking(SPI_PORT,0xff, tmp_rec_array+i,1);
  }
  // Turn sync PIN high to show not ready and input buffer should wait
  gpio_put(SPI_RDY_PIN, 0);
  //printf("Bytes Reads: %d\n",ret);
 // printf("Transfer Complete\n");
  //printf("Rec Array: ");
  // Update global array of observations to push to Neuroprocessor
  for (unsigned int i = 0; i < OBS_ARR_LENGTH; i++){
   //printf("%d ",tmp_rec_array[i]);
   observation_arr[i] = tmp_rec_array[i];
  }

  return;
}

void print_observations(){
    // Clear the console and position the cursor at the top-left corner
    printf("\033[H\033[J");

    for (unsigned int i = 0; i < OBS_ARR_LENGTH; i++) {
        printf("%1d ", observation_arr[i]); // Print each element in a fixed-width format
        if ((i + 1) % 16 == 0) { // Newline every 20 elements for better readability
            printf("\n");
        }
    }
    printf("\n");
  return;
}

/* User defined: Use this procedure to perform any setup needed to read/provide
observation values that are encoded into spikes that are inputted into the
neuroprocessor's network. Called once.
    Arguments:
        - available_pins : Pins available for user to use as desired
        - num_available_pins : Number of pins available for user use
*/

void setup_read_observations(const uint8_t available_pins[], uint8_t num_available_pins) {
  // Setup output LED PIN    
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN,0);

  // Setup SPI control lines (synchronization)
  gpio_init(SPI_RDY_PIN);
  gpio_set_dir(SPI_RDY_PIN, GPIO_OUT);
  gpio_put(SPI_RDY_PIN, 0);

  // Initialize SPI interface
  spi_init(SPI_PORT, 1000 * 1000); // 1000 khz  clock rate
  gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI_TX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI_CS_PIN, GPIO_FUNC_SPI);

  // Set up chip select, operating in slave mode
  spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  spi_set_slave(SPI_PORT, true);

  // Setup output LED PIN
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN,1);
  //printf("IO INITED\n");
  return;
}

/* User defined: Use this procedure to provide an observation value to a spike
encoder that inputs spikes into the neuroprocessor's network. Called repeatedly.
    Arguments:
        - observation_ind : The index of the current observation (equals the
                            index of the current spike encoder)
*/
double loop_read_observation(uint8_t observation_ind) {
  int8_t observation = 0;
  
  if (observation_ind == 0){
    get_observations();
    //print_observations();
  }
 
   // Heartbeat
  gpio_put(LED_PIN, !gpio_get(LED_PIN));  
  return (double) (observation_arr[observation_ind]);

  
}
