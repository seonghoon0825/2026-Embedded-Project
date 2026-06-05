# 임베디드 코드 작성 가이드

이 문서는 `README.md`의 비접촉식 스마트 쓰레기통 프로젝트를 기준으로 ESP32 임베디드 코드를 작성하는 방법을 정리한 문서입니다.

## 1. 프로젝트 동작 목표

스마트 쓰레기통은 사용자가 손이나 물체를 가까이 가져오면 초음파 센서로 접근을 감지하고, 서보 모터로 뚜껑을 자동으로 열고 닫아야 합니다.

기본 동작 흐름은 다음과 같습니다.

```text
대기 상태 -> 거리 측정 -> 접근 감지 -> 뚜껑 열림 -> 일정 시간 유지 -> 뚜껑 닫힘 -> 대기 상태
```

코드는 이 흐름이 명확히 드러나도록 작성해야 하며, 센서 측정 코드와 모터 제어 코드를 분리하는 것이 좋습니다.

## 2. 사용 부품과 핀 설정

현재 코드 기준 핀 설정은 다음과 같습니다.

| 부품 | ESP32 핀 | 용도 |
| --- | --- | --- |
| HC-SR04 Trig | GPIO 5 | 초음파 송신 시작 신호 |
| HC-SR04 Echo | GPIO 18 | 초음파 수신 시간 측정 |
| Servo Signal | GPIO 19 | 서보 모터 각도 제어 |

코드에서는 핀 번호를 직접 반복해서 쓰지 말고 상수로 선언합니다.

```cpp
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int SERVO_PIN = 19;
```

서보 모터는 ESP32에서 일반 `Servo.h` 대신 `ESP32Servo.h`를 사용합니다.

```cpp
#include <ESP32Servo.h>
```

## 3. 코드 기본 구조

ESP32 Arduino 코드는 기본적으로 `setup()`과 `loop()`로 구성합니다.

- `setup()`: 시리얼 통신, 핀 모드, 서보 모터 초기화
- `loop()`: 거리 측정, 상태 판단, 서보 모터 제어 반복

권장 구조는 다음과 같습니다.

```cpp
#include <ESP32Servo.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int SERVO_PIN = 19;

const int DETECT_DISTANCE_CM = 10;
const int OPEN_ANGLE = 100;
const int CLOSE_ANGLE = 50;
const unsigned long OPEN_HOLD_TIME_MS = 3000;

Servo lidServo;

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    lidServo.attach(SERVO_PIN);
    lidServo.write(CLOSE_ANGLE);
}

void loop() {
    long distance = readDistanceCm();

    if (distance > 0 && distance <= DETECT_DISTANCE_CM) {
        openLid();
        delay(OPEN_HOLD_TIME_MS);
        closeLid();
    }

    delay(100);
}
```

## 4. 거리 측정 함수 작성

초음파 센서는 Trig 핀에 10us 이상의 HIGH 펄스를 준 뒤 Echo 핀이 HIGH로 유지된 시간을 측정합니다. 측정된 시간에 음속을 곱해서 거리를 계산합니다.

거리 계산식은 다음과 같습니다.

```text
거리(cm) = Echo 유지 시간(us) * 0.034 / 2
```

함수로 분리하면 `loop()`가 더 읽기 쉬워집니다.

```cpp
long readDistanceCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {
        return -1;
    }

    return duration * 0.034 / 2;
}
```

`pulseIn()`에 timeout을 넣으면 센서가 응답하지 않을 때 코드가 오래 멈추는 문제를 줄일 수 있습니다.

## 5. 서보 모터 제어 함수 작성

뚜껑의 열림 각도와 닫힘 각도는 기구 조립 상태에 따라 달라질 수 있습니다. 현재 코드 기준으로는 열림 각도 `100`, 닫힘 각도 `50`을 사용합니다.

```cpp
void openLid() {
    lidServo.write(OPEN_ANGLE);
}

void closeLid() {
    lidServo.write(CLOSE_ANGLE);
}
```

조립 후에는 다음 순서로 각도를 보정합니다.

1. 닫힘 각도를 먼저 맞춥니다.
2. 뚜껑이 무리 없이 열리는 최대 각도를 찾습니다.
3. 모터가 끝까지 밀고 버티는 소리가 나면 각도 범위를 줄입니다.
4. 실제 쓰레기통 뚜껑 무게를 올린 상태에서 다시 확인합니다.

## 6. 상태 제어 방식

README에는 `감지 -> 열림 -> 유지 -> 닫힘` 상태 제어가 핵심 기술로 정리되어 있습니다. 단순 구현에서는 `delay()`로 유지 시간을 처리할 수 있지만, 기능이 늘어나면 `millis()` 기반 상태 제어를 사용하는 것이 좋습니다.

상태 제어를 사용하면 LED 잔량 표시, 부저 알림 같은 추가 기능을 넣어도 전체 코드가 덜 막힙니다.

```cpp
enum LidState {
    CLOSED,
    OPENED
};

LidState lidState = CLOSED;
unsigned long openedAt = 0;

void loop() {
    long distance = readDistanceCm();
    unsigned long now = millis();

    if (lidState == CLOSED && distance > 0 && distance <= DETECT_DISTANCE_CM) {
        openLid();
        lidState = OPENED;
        openedAt = now;
    }

    if (lidState == OPENED && now - openedAt >= OPEN_HOLD_TIME_MS) {
        closeLid();
        lidState = CLOSED;
    }

    delay(50);
}
```

## 7. 오작동 방지 기준

초음파 센서는 주변 환경, 각도, 반사면에 따라 튀는 값이 나올 수 있습니다. 다음 기준을 코드에 반영하는 것이 좋습니다.

- `0cm` 또는 timeout 값은 잘못된 값으로 처리합니다.
- 너무 큰 값은 무시합니다.
- 한 번 감지했다고 바로 열지 말고 2~3회 연속 감지되었을 때 열면 안정적입니다.
- 시리얼 모니터에 거리 값을 출력해 실제 감지 범위를 확인합니다.

예시:

```cpp
Serial.print("distance: ");
Serial.print(distance);
Serial.println(" cm");
```

## 8. 전원과 하드웨어 주의사항

코드가 정상이어도 전원이 불안정하면 서보 모터가 떨리거나 ESP32가 재부팅될 수 있습니다.

- ESP32는 3.3V 로직으로 동작합니다.
- 서보 모터는 보통 5V 전원을 따로 공급합니다.
- ESP32 GND와 서보 모터 전원 GND는 반드시 공통으로 연결합니다.
- 모터 전원을 ESP32 보드의 3.3V 핀에서 직접 공급하지 않습니다.
- USB 전원만으로 모터를 구동하면 전류 부족이 발생할 수 있습니다.

## 9. 테스트 순서

코드는 한 번에 완성하려고 하지 말고 부품별로 나누어 테스트합니다.

1. 시리얼 모니터가 `115200` baud로 출력되는지 확인합니다.
2. 초음파 센서 거리 값이 손의 위치에 따라 변하는지 확인합니다.
3. 서보 모터가 닫힘 각도와 열림 각도로 정상 회전하는지 확인합니다.
4. 감지 거리 기준을 `10cm`부터 시작해 실제 사용 환경에 맞게 조정합니다.
5. 쓰레기통에 장착한 뒤 뚜껑 무게 때문에 각도가 부족하지 않은지 확인합니다.
6. 장시간 반복 동작 시 ESP32 재부팅이나 모터 과열이 없는지 확인합니다.

## 10. 최종 코드 작성 원칙

- 핀 번호, 거리 기준, 각도, 대기 시간은 상수로 관리합니다.
- 거리 측정, 뚜껑 열기, 뚜껑 닫기는 함수로 분리합니다.
- 센서 오류 값은 반드시 예외 처리합니다.
- 디버깅이 필요할 때는 시리얼 출력으로 실제 값을 확인합니다.
- 추가 기능을 고려한다면 `delay()`보다 `millis()` 기반 상태 제어를 사용합니다.
- 코드 수정 후에는 센서 단독 테스트, 모터 단독 테스트, 통합 테스트 순서로 확인합니다.

## 11. 전체 코드

아래 코드는 현재 프로젝트의 `src/main.cpp`에 들어가는 전체 코드입니다.

```cpp
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
```
