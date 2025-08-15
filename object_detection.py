import cv2
import numpy as np
import urllib.request
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.image import MIMEImage
import time
import os
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# Email configuration
EMAIL_ADDRESS = 'mdsakibsarker52@gmail.com'
EMAIL_PASSWORD = 'acav iegp join llfe'  #App Password,
RECIPIENT_EMAIL = 'shahriarnayem001@gmail.com'
SMTP_SERVER = 'smtp.gmail.com'
SMTP_PORT = 587

# URL for ESP32-CAM (single image for detection)
url = 'http://192.168.55.77/cam-hi.jpg'

# YOLO parameters
whT = 320
confThreshold = 0.5
nmsThreshold = 0.3

# Load class names for YOLO
classesfile = 'coco.names'
classNames = []
with open(classesfile, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

# Load YOLO model
modelConfig = 'yolov3.cfg'
modelWeights = 'yolov3.weights'
net = cv2.dnn.readNetFromDarknet(modelConfig, modelWeights)
net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

# Load Haar Cascade for face detection
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Load reference image for owner
owner_img = cv2.imread('owner.jpeg', cv2.IMREAD_GRAYSCALE)
owner_img = cv2.imread('owner.jpg', cv2.IMREAD_GRAYSCALE)
owner_img = cv2.imread('owner_left.jpg', cv2.IMREAD_GRAYSCALE)
owner_img = cv2.imread('owner_right.jpg', cv2.IMREAD_GRAYSCALE)
owner_img = cv2.imread('owner_smile.jpg', cv2.IMREAD_GRAYSCALE)
if owner_img is None:
    raise FileNotFoundError("Owner image 'owner.jpg' not found.")

# Convert owner image to feature descriptor using ORB
orb = cv2.ORB_create()
owner_kp, owner_desc = orb.detectAndCompute(owner_img, None)

# List of harmful objects
harmful_objects = ['knife', 'gun', 'scissors']

# Email sending function
def send_email(image_path, is_dangerous):
    msg = MIMEMultipart()
    msg['From'] = EMAIL_ADDRESS
    msg['To'] = RECIPIENT_EMAIL
    msg['Subject'] = 'Intruder Alert!'

    body = 'বাইরের লোক ধরা পরেছে ।'
    if is_dangerous:
        body += ' WARNING: বিপদজনক বস্তু!'
    msg.attach(MIMEText(body, 'plain'))

    try:
        with open(image_path, 'rb') as fp:
            img = MIMEImage(fp.read())
            img.add_header('Content-Disposition', 'attachment', filename='intruder.jpg')
            msg.attach(img)

        server = smtplib.SMTP(SMTP_SERVER, SMTP_PORT, timeout=10)
        server.starttls()
        server.login(EMAIL_ADDRESS, EMAIL_PASSWORD)
        server.sendmail(EMAIL_ADDRESS, RECIPIENT_EMAIL, msg.as_string())
        server.quit()
        logging.info("Email sent successfully")
    except Exception as e:
        logging.error(f"Failed to send email: {e}")

def fetch_image_with_retry(url, max_retries=3, timeout=10):
    for attempt in range(max_retries):
        try:
            img_resp = urllib.request.urlopen(url, timeout=timeout)
            imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
            im = cv2.imdecode(imgnp, -1)
            if im is None:
                logging.warning("Failed to decode image")
                continue
            return im
        except Exception as e:
            logging.warning(f"Attempt {attempt + 1}/{max_retries} failed: {e}")
            if attempt < max_retries - 1:
                time.sleep(2)
            continue
    logging.error("All retries failed to fetch image")
    return None

def find_objects_and_faces(im, last_email_time):
    hT, wT, cT = im.shape
    bbox = []
    classIds = []
    confs = []
    is_dangerous = False
    intruder_detected = False

    try:
        blob = cv2.dnn.blobFromImage(im, 1/255, (whT, whT), [0, 0, 0], 1, crop=False)
        net.setInput(blob)
        layernames = net.getLayerNames()
        outputNames = [layernames[i - 1] for i in net.getUnconnectedOutLayers()]
        outputs = net.forward(outputNames)

        for output in outputs:
            for det in output:
                scores = det[5:]
                classId = np.argmax(scores)
                confidence = scores[classId]
                if confidence > confThreshold:
                    w, h = int(det[2] * wT), int(det[3] * hT)
                    x, y = int((det[0] * wT) - w / 2), int((det[1] * hT) - h / 2)
                    bbox.append([x, y, w, h])
                    classIds.append(classId)
                    confs.append(float(confidence))

        indices = cv2.dnn.NMSBoxes(bbox, confs, confThreshold, nmsThreshold)

        for i in indices:
            i = i[0] if isinstance(i, tuple) else i
            box = bbox[i]
            x, y, w, h = box[0], box[1], box[2], box[3]
            class_name = classNames[classIds[i]]

            if class_name in harmful_objects:
                is_dangerous = True
                color = (0, 0, 255)
                label = f'{class_name.upper()} DANGEROUS {int(confs[i] * 100)}%'
            else:
                color = (0, 255, 0)
                label = f'{class_name.upper()} NON-DANGEROUS {int(confs[i] * 100)}%'

            cv2.rectangle(im, (x, y), (x + w, y + h), color, 2)
            cv2.putText(im, label, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
    except Exception as e:
        logging.error(f"YOLO processing error: {e}")

    try:
        gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

        for (x, y, w, h) in faces:
            face_roi = gray[y:y+h, x:x+w]
            face_kp, face_desc = orb.detectAndCompute(face_roi, None)

            is_owner = False
            if face_desc is not None and owner_desc is not None:
                bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
                matches = bf.match(owner_desc, face_desc)
                if len(matches) > 20:
                    is_owner = True

            if is_owner:
                color = (0, 255, 0)
                label = 'OWNER'
            else:
                color = (0, 0, 255)
                label = 'INTRUDER'
                intruder_detected = True

            cv2.rectangle(im, (x, y), (x + w, y + h), color, 2)
            cv2.putText(im, label, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
            logging.info(f"Detected: {label}")
    except Exception as e:
        logging.error(f"Face detection error: {e}")

    current_time = time.time()
    if intruder_detected and (current_time - last_email_time > 60):
        try:
            cv2.imwrite('intruder.jpg', im)
            send_email('intruder.jpg', is_dangerous)
            last_email_time = current_time
            os.remove('intruder.jpg')
        except Exception as e:
            logging.error(f"Error saving/sending image: {e}")

    logging.info('DANGEROUS' if is_dangerous else 'NON-DANGEROUS')
    return im, last_email_time

last_email_time = 0

while True:
    try:
        im = fetch_image_with_retry(url, max_retries=3, timeout=10)
        if im is None:
            time.sleep(1)
            continue

        im, last_email_time = find_objects_and_faces(im, last_email_time)
        cv2.imshow('ESP32-CAM Stream', im)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    except Exception as e:
        logging.error(f"Main loop error: {e}")
        time.sleep(1)

cv2.destroyAllWindows()