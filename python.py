import time
from scipy.spatial import distance
from imutils import face_utils
import imutils
import dlib
import cv2
import requests
import threading



# Drowsiness detection configuration
thresh = 0.25      # Eye Aspect Ratio threshold
frame_check = 10   # Number of consecutive frames below threshold before alert
alert_delay = 3    # Delay in seconds before triggering the alert

# Initialize global variables for timer management
alert_timer = None

# Load dlib's face detector and landmark predictor
detect = dlib.get_frontal_face_detector()
predictor_path = r"C:\Users\ammar\Downloads\facial-landmarks-recognition-master(1)\facial-landmarks-recognition-master\shape_predictor_68_face_landmarks.dat"  # Update with correct path
predict = dlib.shape_predictor(predictor_path)

# Grab the indexes of the facial landmarks for the left and right eye
(lStart, lEnd) = face_utils.FACIAL_LANDMARKS_68_IDXS["left_eye"]
(rStart, rEnd) = face_utils.FACIAL_LANDMARKS_68_IDXS["right_eye"]

# ESP32-CAM video stream and alert URL
esp32_video_url = "http://192.168.137.109/video"   # Update with ESP32-CAM IP
esp32_alert_url = "http://192.168.137.109/alert"   # Update with ESP32-CAM IP

ESP8266_IP = "192.168.137.63"  # Replace with your ESP8266's IP address
ESP8266_PORT = 8080
ESP8266_ENDPOINT = f"http://{ESP8266_IP}:{ESP8266_PORT}/data"

def send_data(eye_level, flag):
    try:
        # Prepare the data as a simple comma-separated string
        data = f"{eye_level},{flag}"
        headers = {'Content-Type': 'text/plain'}
        response = requests.post(ESP8266_ENDPOINT, data=data, headers=headers)
        if response.status_code == 200:
            print("Data sent successfully to ESP8266")
        else:
            print(f"Failed to send data. Status Code: {response.status_code}")
    except Exception as e:
        print(f"Error sending data to ESP8266: {e}")


def eye_aspect_ratio(eye):
    """
    Compute the eye aspect ratio (EAR) for a given eye.
    EAR = (||p2 - p6|| + ||p3 - p5||) / (2 * ||p1 - p4||)
    
    Parameters:
    eye (array): Array of (x, y) coordinates for the eye landmarks.
    
    Returns:
    float: The computed EAR value.
    """
    A = distance.euclidean(eye[1], eye[5])  # p2 - p6
    B = distance.euclidean(eye[2], eye[4])  # p3 - p5
    C = distance.euclidean(eye[0], eye[3])  # p1 - p4
    ear = (A + B) / (2.0 * C)
    return ear


def main():
    global alert_timer
    counter = 0

    cap = cv2.VideoCapture(esp32_video_url)

    if not cap.isOpened():
        print("Error: Unable to open video stream.")
        return

    flag = 0  # Counter for consecutive frames below threshold

    print("Drowsiness detection started. Press 'q' to exit.")

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame")
            break

        frame = imutils.resize(frame, width=450)
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        subjects = detect(gray, 0)

        for subject in subjects:
            shape = predict(gray, subject)
            shape = face_utils.shape_to_np(shape)

            # Extract the left and right eye coordinates
            leftEye = shape[lStart:lEnd]
            rightEye = shape[rStart:rEnd]

            # Compute the eye aspect ratio for both eyes
            leftEAR = eye_aspect_ratio(leftEye)
            rightEAR = eye_aspect_ratio(rightEye)

            # Average the EAR
            ear = (leftEAR + rightEAR) / 2.0

            # Compute the convex hull for the eyes and visualize them
            leftEyeHull = cv2.convexHull(leftEye)
            rightEyeHull = cv2.convexHull(rightEye)
            cv2.drawContours(frame, [leftEyeHull], -1, (0, 255, 0), 1)
            cv2.drawContours(frame, [rightEyeHull], -1, (0, 255, 0), 1)

            # Debugging: Print EAR and flag values
            print(f"EAR: {ear:.3f}, Flag: {flag}")

            # Send data to ESP8266
            send_data(eye_level=ear, flag=flag)

            if ear < thresh:
                flag += 1

                if flag >= frame_check and alert_timer is None:
                    if flag == 10:
                        print("Drowsiness detected!")
                    if flag % 20 == 0 and flag != 0:
                        print("Drowsiness alert triggered!")
            else:
                flag = 0

                # If the eye opens before the alert is triggered, cancel the pending alert
                if alert_timer is not None:
                    print("Eye opened. Canceling scheduled alert.")
                    alert_timer.cancel()
                    alert_timer = None

        # Display the frame with annotations
        cv2.imshow("Frame", frame)
        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break

    # Cleanup: Cancel any pending timer before exiting
    if alert_timer is not None:
        alert_timer.cancel()

    cv2.destroyAllWindows()
    cap.release()


if __name__ == "__main__":
    main()
