# 비접촉식 스마트 쓰레기통 회로도

이 문서는 ESP32, HC-SR04 초음파 센서, 서보 모터를 사용해 쓰레기통 뚜껑을 자동으로 여닫는 기본 회로를 정리한 문서입니다.

시각 회로도 파일은 `docs/circuit_diagram.svg`입니다.

## 1. 사용 부품

| 부품 | 수량 | 용도 |
| --- | ---: | --- |
| ESP32 개발 보드 | 1 | 센서 측정 및 서보 모터 제어 |
| HC-SR04 초음파 센서 | 2 | 사용자 접근 거리 측정 (외부) + 잔량 측정 (내부) |
| 서보 모터 | 1 | 쓰레기통 뚜껑 개폐 |
| LED (초록/노랑/빨강) | 3 | 잔량 표시 |
| 수동 부저 | 1 | 가득 찼을 때 경고 |
| **1602 LCD I2C 모듈** | **1** | **상태 표시 디스플레이** |
| 저항 1k ohm | 1 | 외부 Echo 전압 분배 |
| 저항 2k ohm | 1 | 외부 Echo 전압 분배 |
| 저항 220 ohm | 3 | LED 전류 제한 |
| 5V 외부 전원 | 1 | 서보 모터 전원 공급 |
| 점퍼선 | 필요 수량 | 부품 연결 |

## 2. 전체 연결도

```text
                         +---------------------+
                         |       ESP32         |
                         |                     |
EXT HC-SR04 VCC ---------+ 5V 또는 VIN        |
EXT HC-SR04 GND ---------+ GND                |
EXT HC-SR04 TRIG --------+ GPIO 5             |
EXT HC-SR04 ECHO -[1k]---+ GPIO 18            |
                  |      |                     |
                 [2k]    |                     |
                  |      |                     |
GND ---------------+-----+ GND                |
                         |                     |
INT HC-SR04 VCC ---------+ 5V 또는 VIN        |
INT HC-SR04 GND ---------+ GND                |
INT HC-SR04 TRIG --------+ GPIO 17            |
INT HC-SR04 ECHO -[1k]---+ GPIO 16            |
                  |      |                     |
                 [2k]    |                     |
                  |      |                     |
GND ---------------+-----+ GND                |
                         |                     |
LED Green --[220Ω]------+ GPIO 12             |
LED Yellow -[220Ω]------+ GPIO 13             |
LED Red ----[220Ω]------+ GPIO 14             |
(모든 LED 음극) --------+ GND                |
                         |                     |
Buzzer +  ---------------+ GPIO 27             |
Buzzer -  ---------------+ GND                |
                         |                     |
Servo Signal -----------+ GPIO 19             |
                         |                     |
LCD I2C Module:         |                     |
  SDA -------------------+ GPIO 21 (SDA)      |
  SCL -------------------+ GPIO 22 (SCL)      |
  VCC -------------------+ 5V 또는 VIN        |
  GND -------------------+ GND                |
                         +---------------------+

External 5V (+) --------- Servo VCC
External 5V (-) --------- Servo GND + ESP32 GND
```

## 3. 핀 연결표

### 3.1 외부 초음파 센서 (손 감지)

| 부품 핀 | 연결 위치 | 설명 |
| --- | --- | --- |
| HC-SR04 VCC | ESP32 5V 또는 VIN | 외부 초음파 센서 전원 |
| HC-SR04 GND | ESP32 GND | 외부 초음파 센서 접지 |
| HC-SR04 TRIG | ESP32 GPIO 5 | 외부 초음파 송신 시작 신호 |
| HC-SR04 ECHO | 저항 분배 후 ESP32 GPIO 18 | 외부 초음파 반사 시간 입력 |

### 3.2 내부 초음파 센서 (잔량 측정)

| 부품 핀 | 연결 위치 | 설명 |
| --- | --- | --- |
| HC-SR04 VCC | ESP32 5V 또는 VIN | 내부 초음파 센서 전원 |
| HC-SR04 GND | ESP32 GND | 내부 초음파 센서 접지 |
| HC-SR04 TRIG | ESP32 GPIO 17 | 내부 초음파 송신 시작 신호 |
| HC-SR04 ECHO | 저항 분배 후 ESP32 GPIO 16 | 내부 초음파 반사 시간 입력 |

### 3.3 서보 모터

| 부품 핀 | 연결 위치 | 설명 |
| --- | --- | --- |
| Servo Signal | ESP32 GPIO 19 | 서보 모터 PWM 제어 신호 |
| Servo VCC | 외부 5V + | 서보 모터 전원 |
| Servo GND | 외부 5V - 및 ESP32 GND | 공통 접지 |

### 3.4 LED 표시 (잔량 상태)

| 부품 | 핀 배치 | 설명 |
| --- | --- | --- |
| LED Green (충분함) | GPIO 12 + [220Ω] + GND | 쓰레기 적음 |
| LED Yellow (보통) | GPIO 13 + [220Ω] + GND | 쓰레기 보통 |
| LED Red (거의 찼음) | GPIO 14 + [220Ω] + GND | 쓰레기 많음 (경고) |

### 3.5 부저 (가득 참 경고)

| 부품 | 연결 위치 | 설명 |
| --- | --- | --- |
| Buzzer + (VCC) | ESP32 GPIO 27 | 부저 신호 제어 |
| Buzzer - (GND) | ESP32 GND | 부저 접지 |

### 3.6 LCD I2C 모듈 (1602 디스플레이)

| 부품 핀 | 연결 위치 | 설명 |
| --- | --- | --- |
| LCD VCC | ESP32 5V 또는 VIN | LCD 모듈 전원 |
| LCD GND | ESP32 GND | LCD 모듈 접지 |
| LCD SDA | ESP32 GPIO 21 | I2C 데이터 신호 |
| LCD SCL | ESP32 GPIO 22 | I2C 클록 신호 |

**주의:** I2C 주소 확인 필요 (기본값: 0x27 또는 0x3F)

## 4. Echo 전압 분배 회로

HC-SR04의 Echo 핀은 일반적으로 5V 신호를 출력합니다. ESP32 GPIO는 3.3V 입력을 사용하므로 Echo 신호를 그대로 연결하지 말고 저항 2개로 전압을 낮춥니다. 이는 **두 개의 초음파 센서 모두**에 적용됩니다.

### 4.1 외부 센서 (손 감지) Echo 핀 분배

```text
EXT HC-SR04 Echo ----[1k ohm]----+---- ESP32 GPIO 18
                                  |
                                [2k ohm]
                                  |
                                 GND
```

### 4.2 내부 센서 (잔량 측정) Echo 핀 분배

```text
INT HC-SR04 Echo ----[1k ohm]----+---- ESP32 GPIO 16
                                  |
                                [2k ohm]
                                  |
                                 GND
```

이 연결은 약 5V Echo 신호를 약 3.3V로 낮춰 ESP32 입력 핀을 보호합니다.

## 5. 전원 연결 주의사항

- 서보 모터 전원은 ESP32의 3.3V 핀에서 직접 공급하지 않습니다.
- 서보 모터는 순간 전류가 크므로 가능하면 별도의 5V 전원을 사용합니다.
- 외부 5V 전원의 GND와 ESP32 GND는 반드시 연결해야 합니다.
- GND를 공통으로 연결하지 않으면 PWM 신호 기준이 맞지 않아 서보 모터가 떨리거나 동작하지 않을 수 있습니다.
- USB 전원만으로 서보 모터를 구동하면 ESP32가 재부팅될 수 있습니다.

## 6. 코드 기준 핀 설정

`src/main.cpp`의 핀 설정은 다음과 같습니다.

### 6.1 외부 센서 (손 감지)
```cpp
const int EXT_TRIG_PIN = 5;
const int EXT_ECHO_PIN = 18;
```

### 6.2 내부 센서 (잔량 측정)
```cpp
const int INT_TRIG_PIN = 17;
const int INT_ECHO_PIN = 16;
```

### 6.3 서보 모터
```cpp
const int SERVO_PIN = 19;
```

### 6.4 LED 표시
```cpp
const int LED_GREEN_PIN = 12;    // 충분함
const int LED_YELLOW_PIN = 13;   // 보통
const int LED_RED_PIN = 14;      // 거의 찼음 (경고)
```

### 6.5 부저
```cpp
const int BUZZER_PIN = 27;
```

### 6.6 LCD I2C 모듈
```cpp
const int LCD_ADDR = 0x27;  // I2C 주소 (0x27 또는 0x3F)
const int LCD_COLS = 16;    // LCD 열 수
const int LCD_ROWS = 2;     // LCD 행 수
// I2C: SDA = GPIO 21, SCL = GPIO 22
```

### 6.7 잔량 기준값
```cpp
const float TRASH_CAN_HEIGHT_CM = 30.0;  // 쓰레기통 높이
const float FULL_THRESHOLD_CM = 5.0;     // 가득 참 (상단에서 5cm)
const float YELLOW_THRESHOLD_CM = 12.0;  // 노랑 (상단에서 12cm)
```

회로의 GPIO 번호를 바꾸면 코드의 핀 상수도 함께 수정해야 합니다.

## 7. 동작 확인 순서

### Phase 1: 기본 센서 테스트
1. ESP32와 **외부 HC-SR04만** 연결한 뒤 시리얼 모니터에서 거리 값이 변하는지 확인합니다.
2. 손을 센서 앞 10cm 이내로 가져갔을 때 거리 값이 감소하는지 확인합니다.

### Phase 2: 내부 센서 추가
1. **내부 HC-SR04**를 추가로 연결합니다.
2. 시리얼 모니터에서 내부 센서의 거리 값이 나오는지 확인합니다.
3. 내부 센서 앞에 물체를 가깝게 대었을 때 거리 값이 감소하는지 확인합니다.

### Phase 3: 서보 모터 테스트
1. 서보 모터를 외부 5V 전원에 연결하고 GND를 ESP32와 공통으로 묶습니다.
2. 손을 센서 앞 10cm 이내로 가져갔을 때 서보 모터가 열림 각도로 움직이는지 확인합니다.
3. 손을 치운 뒤 약 3초 후 닫힘 각도로 돌아오는지 확인합니다.
4. 뚜껑을 장착한 뒤 `OPEN_ANGLE`, `CLOSE_ANGLE` 값을 실제 구조에 맞게 조정합니다.

### Phase 4: LED 표시 테스트
1. LED 3개(초록, 노랑, 빨강)를 각각 GPIO 12, 13, 14에 연결합니다.
2. 내부 센서 앞의 거리를 변경하면서 LED가 바뀌는지 확인합니다:
   - `12cm` 이상: 초록 LED 켜짐 (EMPTY)
   - `5~12cm`: 노랑 LED 켜짐 (NORMAL)
   - `5cm` 이하: 빨강 LED 켜짐 (FULL)

### Phase 5: 부저 경고 테스트
1. 부저를 GPIO 27에 연결합니다.
2. 내부 센서 앞의 거리가 `5cm` 이하가 되면 부저가 울리는지 확인합니다.
3. 부저는 약 1초 동안 울린 후 멈춥니다.

### Phase 6: 전체 통합 테스트
1. 모든 부품을 연결한 상태에서:
   - 손을 센서 앞에 가져가면 뚜껑이 열림
   - 손을 치우면 약 3초 후 뚜껑이 닫힘
   - 내부 잔량에 따라 LED 색상이 변함
   - 가득 찰 시 부저 경고음이 울림
