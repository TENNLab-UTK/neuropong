import dv_processing as dv
import downsample
import time
from datetime import timedelta
import cv2 as cv
import argparse
import sys
import ast
import numpy as np
import math
import spidev
import RPi.GPIO as GPIO 

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NeuroPong Event Server")
    parser.add_argument("--crop","-c",type=int,nargs='+',help="4 values for the crop: x, y, width, height.")
    parser.add_argument("--viz","-v",type=ast.literal_eval,default=False,help="Whether or not to display frames using openCV.")
    parser.add_argument("--time_segmentation_length","-tsl",default=33000,type=int,help="Length of accumulation window in microseconds to divide events into. ")
    parser.add_argument('--downsampling_params','-dp', nargs='+', type=int, help="Downsampling parameters in the following order: row, col, row_stride, col_stride, threshold. You can specify multiple layers, but they must be provided in multiples of 5.")
    args = parser.parse_args()

    if args.crop and (len(args.crop) != 4 and len(args.crop) != 0): 
        print("ERROR: args.crop needs to have either 4 values or 0 values.")
        sys.exit()

    event_source = dv.io.CameraCapture()
    microsecond_offset = 0

    if args.downsampling_params and len(args.downsampling_params) > 0 and len(args.downsampling_params) % 5 != 0:
        print("Bad downsampling parameters. You must have 5 per downsampling pass, specified as: row, col, row_stride, col_stride, threshold.")
        exit()
    elif args.downsampling_params:
        #For downsampling parameters
        start_row = 0
        end_row = 260
        start_col = 0
        end_col = 346
        if args.crop and len(args.crop) > 0:
            start_row = args.crop[1]
            end_row = args.crop[1] + args.crop[3]
            start_col = args.crop[0]
            end_col = args.crop[0] + args.crop[2]

        tmp_end_row = end_row
        tmp_end_col = end_col
        tmp_start_row = start_row
        tmp_start_col = start_col
        for i in range(0,int(len(args.downsampling_params) / 5)):
            new_col = math.ceil((tmp_end_col - tmp_start_col)/args.downsampling_params[i * 5 + 3])
            new_row = math.ceil((tmp_end_row - tmp_start_row)/args.downsampling_params[i * 5 + 2])
            print("Downsampling %dx%d --> %dx%d"%(tmp_end_col,tmp_end_row,new_col,new_row))
            tmp_end_col = new_col
            tmp_end_row = new_row
            tmp_start_row = 0
            tmp_start_col = 0


    # Crop filter -- (x, y, width, height) --> x=0, y=0 is top left corner of screen
    if args.crop and len(args.crop) == 4:
        filter = dv.EventRegionFilter(tuple(args.crop))
                                        

    # Initialize SPI communication variables
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(21, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    spi = spidev.SpiDev()
    spi.open(0, 0)
    spi.max_speed_hz = 1000 * 1000
    spi.mode = 0b00
    spi.bits_per_word = 8
    spi.lsbfirst = False

    # Stream interval defines the packet frequency for this sample
    streamInterval = timedelta(milliseconds=10)
    
    event_buffer = dv.EventStore()
    while event_source.isRunning():
        #Grab batch of events
        events = event_source.getNextEventBatch()

        # If events is empty, give it a second.. (technically, 10/1000ths of a second)
        if events is None:
            time.sleep(streamInterval.total_seconds())
            continue
        
        #Catch up our event buffer by grabbing events available up to our current time from the camera buffer
        while(time.time_ns() // 1000 - microsecond_offset) > events.getHighestTime():
            next_batch = event_source.getNextEventBatch()
            if next_batch is None:
                break
            events.add(next_batch)
           

        # Add recently grabbed events to the buffer
        event_buffer.add(events)

        # Grab the event buffer times at each end
        event_buffer_high_time = event_buffer.getHighestTime() 
        event_buffer_low_time = event_buffer.getLowestTime() 
        offset = 0

        # If the difference between the events is not AT LEAST the size of one whole observation, grab the next event batch
        if (event_buffer_high_time - event_buffer_low_time) >= int(args.time_segmentation_length):

            # Apply the desired crop after we say we have enough events for an observation..Should give us more sparse observations and be more realistic.
            if ((args.crop and len(args.crop) == 4)):
                filter.accept(event_buffer)
                # Call generate events to apply the filter
                event_buffer = filter.generateEvents()

            # If there is enough in the buffer for one whole observation, slice it up starting from the most recent event back to the event occuring one observation's worth of time earlier
            observation = event_buffer.sliceTime(event_buffer_high_time - int(args.time_segmentation_length),event_buffer_high_time)
            observation_low = observation.getLowestTime()
            observation_high = observation.getHighestTime() 

            if args.downsampling_params and len(args.downsampling_params) > 0:
                # Build a temporary event frame
                frame = np.zeros((260,346), dtype=int)
                
                for event in observation.numpy():
                    frame[event[2]][event[1]] = 1

                if args.crop and len(args.crop) > 0:
                    frame = downsample.downsample(frame, args.downsampling_params[0], args.downsampling_params[1], args.downsampling_params[2], args.downsampling_params[3], args.downsampling_params[4], args.crop[0], args.crop[1], args.crop[2], args.crop[3]) 
                else: 
                    frame = downsample.downsample(frame, args.downsampling_params[0], args.downsampling_params[1], args.downsampling_params[2], args.downsampling_params[3], args.downsampling_params[4], 0, 0, 0, 0)
                
                '''
                #For downsampling params
                start_row = 0
                end_row = 260
                start_col = 0
                end_col = 346
                if args.crop and len(args.crop) > 0:
                    start_row = args.crop[1]
                    end_row = args.crop[1] + args.crop[3]
                    start_col = args.crop[0]
                    end_col = args.crop[0] + args.crop[2]

                for z in range(0,int(len(args.downsampling_params) / 5)):
                    downsampled_observation = []
                    for i in range(start_row, end_row, args.downsampling_params[z * 5 + 2]):
                        tmp_row = []
                        for j in range(start_col, end_col, args.downsampling_params[z * 5 + 3]):
                            num_events = 0
                            for y in range(i,args.downsampling_params[z * 5 + 0] + i):
                                for x in range(j,args.downsampling_params[z * 5 + 1] + j):
                                    if y >= end_row or x >= end_col: continue
                                    
                                    if frame[y][x] == 1:
                                        num_events += 1
                            if num_events > args.downsampling_params[z * 5 + 4]:
                                tmp_row.append(1)
                            else:
                                tmp_row.append(0)
                        downsampled_observation.append(tmp_row)
                        
                    end_col = math.ceil((end_col - start_col)/args.downsampling_params[z * 5 + 3])
                    end_row = math.ceil((end_row - start_row)/args.downsampling_params[z * 5 + 2]) 
                    start_col = 0
                    start_row = 0
                    frame = downsampled_observation
                #print(np.asarray(frame))   
            #    of.write(np.array2string(np.asarray(frame)))
            #   of.write('\n')
                '''                
                
                if (GPIO.input(21)):
                    for x in range(len(frame)):
                        for y in range(len(frame[x])):
                            spi.xfer([frame[x][y]])

                if args.viz == True:
                    viz_frame = ((np.array(frame)) * 255).astype(np.uint8)
                    cv.namedWindow("Preview", cv.WINDOW_NORMAL)
                    cv.imshow("Preview", viz_frame)
                    cv.waitKey(2)
            

            event_buffer = dv.EventStore()
