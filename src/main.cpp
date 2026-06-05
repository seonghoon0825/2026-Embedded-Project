#include <ESP32Servo.h>

// 외부 센서 (손 감지)
const int EXT_TRIG_PIN = 5;
const int EXT_ECHO_PIN = 18;

// 내부 센서 (잔량 측정)
const int INT_TRIG_PIN = 17;
const int INT_ECHO_PIN = 16;

// 서보 모터
const int SERVO_PIN = 19;

// LED 핀 (잔량 표시)
const int LED_GREEN_PIN = 12;    // 충분함 (초록)
const int LED_YELLOW_PIN = 13;   // 보통 (노랑)
const int LED_RED_PIN = 14;      // 거의 찼음 (빨강)

// 부저 핀
const int BUZZER_PIN = 27;

const float SOUND_SPEED_CM_PER_US = 0.0343;
const unsigned long ECHO_TIMEOUT_US = 30000;

// 외부 센서 (손 감지)
const float DETECT_DISTANCE_CM = 10.0;
const int REQUIRED_DETECT_COUNT = 2;

// 내부 센서 (잔량 측정) - 쓰레기통 높이 기준
const float TRASH_CAN_HEIGHT_CM = 30.0;  // 쓰레기통 높이 (cm)
const float FULL_THRESHOLD_CM = 5.0;     // 가득 참 (상단에서 5cm)
const float YELLOW_THRESHOLD_CM = 12.0;  // 노랑 (상단에서 12cm)

const int CLOSE_ANGLE = 50;
const int OPEN_ANGLE = 100;
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;

const unsigned long SAMPLE_INTERVAL_MS = 60;
const unsigned long OPEN_HOLD_TIME_MS = 3000;
const unsigned long SERIAL_PRINT_INTERVAL_MS = 500;
const unsigned long BUZZER_DURATION_MS = 1000;

enum LidState {
    CLOSED,
    OPENED
};

enum TrashLevel {
    EMPTY,      // 초록 - 충분함
    NORMAL,     // 노랑 - 보통
    FULL        // 빨강 - 거의 찼음
};

Servo lidServo;
LidState lidState = CLOSED;
TrashLevel trashLevel = EMPTY;

unsigned long lastSampleAt = 0;
unsigned long openedAt = 0;
unsigned long lastSerialPrintAt = 0;
unsigned long buzzerStartAt = 0;
int stableDetectCount = 0;
bool buzzerActive = false;

float readDistanceCm(int trigPin, int echoPin);
void updateTrashLevel(float internalDistanceCm);
void updateLED();
void updateBuzzer(unsigned long now);
void openLid(unsigned long now);
void closeLid();
void stopBuzzer();
void printStatus(float externalDistanceCm, float internalDistanceCm);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // 외부 센서 (손 감지) 설정
    pinMode(EXT_TRIG_PIN, OUTPUT);
    pinMode(EXT_ECHO_PIN, INPUT);
    digitalWrite(EXT_TRIG_PIN, LOW);

    // 내부 센서 (잔량 측정) 설정
    pinMode(INT_TRIG_PIN, OUTPUT);
    pinMode(INT_ECHO_PIN, INPUT);
    digitalWrite(INT_TRIG_PIN, LOW);

    // LED 설정
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_YELLOW_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);

    // 부저 설정
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // 서보 모터 설정
    lidServo.setPeriodHertz(50);
    lidServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    closeLid();

    Serial.println("\n===========================================");
    Serial.println("Smart Trash Can Controller Started");
    Serial.println("===========================================");
    Serial.println("Sensors: External (hand detection) + Internal (trash level)");
    Serial.println("Controls: Servo + LED indicator + Buzzer alarm");
    Serial.println("===========================================\n");
}

void loop() {
    unsigned long now = millis();

    if (now - lastSampleAt < SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSampleAt = now;

    // 손 감지 (외부 센서)
    float externalDistanceCm = readDistanceCm(EXT_TRIG_PIN, EXT_ECHO_PIN);
    bool objectDetected = externalDistanceCm > 0 && externalDistanceCm <= DETECT_DISTANCE_CM;

    if (objectDetected) {
        if (stableDetectCount < REQUIRED_DETECT_COUNT) {
            stableDetectCount++;
        }
    } else {
        stableDetectCount = 0;
    }

    // 뚜껑 개폐 제어
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

    // 잔량 측정 (내부 센서)
    float internalDistanceCm = readDistanceCm(INT_TRIG_PIN, INT_ECHO_PIN);
    updateTrashLevel(internalDistanceCm);

    // LED 업데이트
    updateLED();

    // 부저 업데이트
    updateBuzzer(now);

    // 시리얼 모니터에 상태 출력
    if (now - lastSerialPrintAt >= SERIAL_PRINT_INTERVAL_MS) {
        lastSerialPrintAt = now;
        printStatus(externalDistanceCm, internalDistanceCm);
    }
}

float readDistanceCm(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);

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
    Serial.println(">>> Lid OPENED");
}

void closeLid() {
    lidServo.write(CLOSE_ANGLE);
    lidState = CLOSED;
    stableDetectCount = 0;
    Serial.println(">>> Lid CLOSED");
}

void updateTrashLevel(float internalDistanceCm) {
    // internalDistanceCm: 센서에서 쓰레기 표면까지의 거리
    // FULL_THRESHOLD_CM보다 작으면 = 쓰레기가 많음 = 가득 찼음
    
    if (internalDistanceCm < 0) {
        // 센서 오류
        trashLevel = EMPTY;
        return;
    }

    if (internalDistanceCm <= FULL_THRESHOLD_CM) {
        trashLevel = FULL;
    } else if (internalDistanceCm <= YELLOW_THRESHOLD_CM) {
        trashLevel = NORMAL;
    } else {
        trashLevel = EMPTY;
    }
}

void updateLED() {
    // 모든 LED를 먼저 끔
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_YELLOW_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);

    // 상태에 따라 해당 LED만 켬
    switch (trashLevel) {
        case EMPTY:
            digitalWrite(LED_GREEN_PIN, HIGH);
            break;
        case NORMAL:
            digitalWrite(LED_YELLOW_PIN, HIGH);
            break;
        case FULL:
            digitalWrite(LED_RED_PIN, HIGH);
            // FULL 상태면 부저도 시작
            if (!buzzerActive) {
                buzzerActive = true;
                buzzerStartAt = millis();
            }
            break;
    }
}

void updateBuzzer(unsigned long now) {
    if (!buzzerActive) {
        return;
    }

    if (now - buzzerStartAt < BUZZER_DURATION_MS) {
        // 부저 울리는 중
        digitalWrite(BUZZER_PIN, HIGH);
    } else {
        // 부저 끔
        stopBuzzer();
    }
}

void stopBuzzer() {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
}

void printStatus(float externalDistanceCm, float internalDistanceCm) {
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.print("s] ");

    // 외부 센서 정보
    Serial.print("Hand: ");
    if (externalDistanceCm < 0) {
        Serial.print("timeout");
    } else {
        Serial.print(externalDistanceCm);
        Serial.print("cm");
    }

    Serial.print(" | Lid: ");
    Serial.print(lidState == OPENED ? "OPEN" : "CLOSED");

    // 내부 센서 정보
    Serial.print(" | Trash: ");
    if (internalDistanceCm < 0) {
        Serial.print("timeout");
    } else {
        Serial.print(internalDistanceCm);
        Serial.print("cm");
    }

    Serial.print(" | Level: ");
    switch (trashLevel) {
        case EMPTY:
            Serial.print("EMPTY (Green)");
            break;
        case NORMAL:
            Serial.print("NORMAL (Yellow)");
            break;
        case FULL:
            Serial.print("FULL (Red) [ALARM]");
            break;
    }

    Serial.println();
}
