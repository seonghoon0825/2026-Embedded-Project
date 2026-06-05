#include <ESP32Servo.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int SERVO_PIN = 19;

const float SOUND_SPEED_CM_PER_US = 0.0343;
const unsigned long ECHO_TIMEOUT_US = 30000;

const float DETECT_DISTANCE_CM = 10.0;
const int REQUIRED_DETECT_COUNT = 2;

const int CLOSE_ANGLE = 50;
const int OPEN_ANGLE = 100;
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;

const unsigned long SAMPLE_INTERVAL_MS = 60;
const unsigned long OPEN_HOLD_TIME_MS = 3000;
const unsigned long SERIAL_PRINT_INTERVAL_MS = 500;

enum LidState {
    CLOSED,
    OPENED
};

Servo lidServo;
LidState lidState = CLOSED;

unsigned long lastSampleAt = 0;
unsigned long openedAt = 0;
unsigned long lastSerialPrintAt = 0;
int stableDetectCount = 0;

float readDistanceCm();
void openLid(unsigned long now);
void closeLid();
void printStatus(float distanceCm);

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    lidServo.setPeriodHertz(50);
    lidServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    closeLid();

    Serial.println("Smart trash can controller started.");
}

void loop() {
    unsigned long now = millis();

    if (now - lastSampleAt < SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSampleAt = now;

    float distanceCm = readDistanceCm();
    bool objectDetected = distanceCm > 0 && distanceCm <= DETECT_DISTANCE_CM;

    if (objectDetected) {
        if (stableDetectCount < REQUIRED_DETECT_COUNT) {
            stableDetectCount++;
        }
    } else {
        stableDetectCount = 0;
    }

    if (lidState == CLOSED && stableDetectCount >= REQUIRED_DETECT_COUNT) {
        openLid(now);
    }

    if (lidState == OPENED) {
        if (objectDetected) {
            openedAt = now;
        }

        if (now - openedAt >= OPEN_HOLD_TIME_MS) {
            closeLid();
        }
    }

    if (now - lastSerialPrintAt >= SERIAL_PRINT_INTERVAL_MS) {
        lastSerialPrintAt = now;
        printStatus(distanceCm);
    }
}

float readDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);

    if (duration == 0) {
        return -1.0;
    }

    return duration * SOUND_SPEED_CM_PER_US / 2.0;
}

void openLid(unsigned long now) {
    lidServo.write(OPEN_ANGLE);
    lidState = OPENED;
    openedAt = now;
    stableDetectCount = 0;
}

void closeLid() {
    lidServo.write(CLOSE_ANGLE);
    lidState = CLOSED;
    stableDetectCount = 0;
}

void printStatus(float distanceCm) {
    Serial.print("distance: ");

    if (distanceCm < 0) {
        Serial.print("timeout");
    } else {
        Serial.print(distanceCm);
        Serial.print(" cm");
    }

    Serial.print(", state: ");
    Serial.println(lidState == OPENED ? "opened" : "closed");
}
