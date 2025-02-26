import cv2
import os
import math
import dv_processing as dv

# Target downsampling parameters (this needs to match the training input shape for the SNN)
target_row = 10
target_col = 16
target_threshold = 35 # This value must be experimentally determined
target_framerate = 30 # This value needs to match training framerate

# Step 1: Initialize camera feed (change the argument to 0 or 1 depending on your webcam ID)
cap = dv.io.CameraCapture()
os.environ['XDG_SESSION_TYPE']='x11'
print(f"Opened [{cap.getCameraName()}] camera")
print("1. Calibrate the focus, aperture, and zoom on the event-camera by turning the rings on the lens")
print("2. Once image is in focus, PRESS 'r' to select ROI")

# Step 2: Capture the camera feed and allow the user to select ROI
while cap.isRunning():
    # Read a frame from the camera
    frame = cap.getNextFrame()

    if frame is not None:
        # Display the camera feed
        cv2.imshow("Event-Camera Feed", frame.image)

    # Allow for realtime iteraction (press 'q' to quit, or select ROI)
    key = cv2.waitKey(50) & 0xFF
    if key == ord('q'):
        exit()
    elif key == ord('r'):
        break

# Step 3: Once user presses 'r', select the ROI
print("")
cv2.destroyAllWindows()
roi = cv2.selectROI("Select Region of Interest", frame.image, showCrosshair=True, fromCenter=False)

# Crop the image to the selected ROI
x, y, w, h = roi
# Adjust ROI dimensions ot ensure divisibility by target dimensions
h = h // target_row * target_row
w = w // target_col * target_col

cropped_image = frame.image[int(y):int(y+h), int(x):int(x+w)]

# Calculate event_server.py parameters
row_stride = (h // target_row)
col_stride = (w // target_col)
time_segment_length_us = math.ceil( (1/target_framerate) * 1000 * 1000) # Microseconds

# Display the cropped region
cv2.destroyAllWindows()
cv2.imshow("Cropped Image", cropped_image)

# Wait for any key to close the window
cv2.waitKey(0)
print("CALIBRATED event_server.py COMMAND TO RUN:")
print("python3 event_server.py ", end=" ")
print(f"--crop {x} {y} {w} {h}", end=" ")
print(f"-tsl {time_segment_length_us}", end=" ")
print(f"-dp {row_stride} {col_stride} {row_stride} {col_stride} {target_threshold}")

# Release the camera and close windows
cv2.destroyAllWindows()

