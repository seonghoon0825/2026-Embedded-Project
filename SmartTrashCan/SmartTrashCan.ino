#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ==============================
// Wi-Fi / UDP 설정
// ==============================
const char* WIFI_SSID = "GalaxyS21+5G0523";
const char* WIFI_PASSWORD = "2271106377";

// 핸드폰이 핫스팟이므로 핸드폰 IP = 게이트웨이 IP (자동 인식)
// 앱에서 열어 둘 UDP 포트
const uint16_t PHONE_PORT = 5005;
const char* DEVICE_ID = "esp32-trashcan";
const unsigned long UDP_SEND_INTERVAL_MS = 1000; // 전송 주기 1초

WiFiUDP udp;
unsigned long lastUdpSendAt = 0;
unsigned long udpSequence = 0;
unsigned long lastWifiRetryAt = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 300000; // Wi-Fi 재연결 시도 간격 (5분)

// I2C LCD 설정 (PCF8574AT 백팩 모듈)
const int LCD_SDA = 4;
const int LCD_SCL = 3;
const int LCD_ADDR = 0x27;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// 외부 초음파 센서 (손 감지)
const int EXT_TRIG_PIN = 6;
const int EXT_ECHO_PIN = 7;

// 내부 초음파 센서 (잔량 측정)
const int INT_TRIG_PIN = 5;
const int INT_ECHO_PIN = 47;

// 서보 모터
const int SERVO_PIN = 37;

// 조도센서
const int LIGHT_AO_PIN = 2;
const int LIGHT_DO_PIN = 1;

const float SOUND_SPEED_CM_PER_US = 0.0343;
const unsigned long ECHO_TIMEOUT_US = 30000;

// 외부 센서 (손 감지)
const float DETECT_DISTANCE_CM = 10.0;
const int REQUIRED_DETECT_COUNT = 2;

// 내부 센서 (잔량 측정) - 쓰레기통 높이 기준
const float TRASH_CAN_HEIGHT_CM = 30.0;
const float FULL_THRESHOLD_CM = 5.0;
const float YELLOW_THRESHOLD_CM = 12.0;

const int CLOSE_ANGLE = 180;
const int OPEN_ANGLE = 90;
const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;
const int SERVO_STEP_DELAY_MS = 15; // 뚜껑 열림/닫힘 속도 (값이 클수록 천천히)

const unsigned long SAMPLE_INTERVAL_MS = 60;
const unsigned long OPEN_HOLD_TIME_MS = 3000;
const unsigned long SERIAL_PRINT_INTERVAL_MS = 500;
const unsigned long LCD_UPDATE_INTERVAL_MS = 200;

enum LidState {
    CLOSED,
    OPENED
};

enum TrashLevel {
    EMPTY,
    NORMAL,
    FULL
};

Servo lidServo;
LidState lidState = CLOSED;
TrashLevel trashLevel = EMPTY;

unsigned long lastSampleAt = 0;
unsigned long openedAt = 0;
unsigned long lastSerialPrintAt = 0;
unsigned long lastLcdUpdateAt = 0;
int stableDetectCount = 0;
int lightAnalogValue = 0;
bool lightDigitalValue = false;

unsigned long lastOpenTimeSec = 0; // 모터(뚜껑)가 마지막으로 열린 시각 (부팅 후 초)
int trashFillPercent = 0;          // 쓰레기 잔량 (0~100%)

float readDistanceCm(int trigPin, int echoPin);
void updateTrashLevel(float internalDistanceCm);
void readLightSensor();
void updateLcd(float externalDistanceCm, float internalDistanceCm);
void openLid(unsigned long now);
void closeLid();
void printStatus(float externalDistanceCm, float internalDistanceCm);
void connectWiFi();
void sendUdpStatus();
void receiveUdpCommand(unsigned long now);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // I2C LCD 초기화
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();
    lcd.print("Smart Trash Can");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    delay(2000);
    lcd.clear();

    // 외부 센서 (손 감지) 설정
    pinMode(EXT_TRIG_PIN, OUTPUT);
    pinMode(EXT_ECHO_PIN, INPUT);
    digitalWrite(EXT_TRIG_PIN, LOW);

    // 내부 센서 (잔량 측정) 설정
    pinMode(INT_TRIG_PIN, OUTPUT);
    pinMode(INT_ECHO_PIN, INPUT);
    digitalWrite(INT_TRIG_PIN, LOW);

    // 조도센서 설정
    pinMode(LIGHT_DO_PIN, INPUT);

    // 서보 모터 설정
    lidServo.setPeriodHertz(50);
    lidServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    closeLid();

    // Wi-Fi 연결 및 UDP 시작
    connectWiFi();
    if (udp.begin(4000)) {
        Serial.println("[UDP] 로컬 UDP 포트 4000 시작");
    } else {
        Serial.println("[UDP] UDP 초기화 실패");
    }

    Serial.println("\n===========================================");
    Serial.println("Smart Trash Can Controller Started");
    Serial.println("===========================================");
    Serial.println("Board: Wi-Fi LoRa 32 V3");
    Serial.println("Sensors: Ultrasonic x2 + Light sensor");
    Serial.println("Output: Servo + LCD 1602 (4-bit parallel)");
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

    // 조도센서 읽기
    readLightSensor();

    // 시리얼 모니터에 상태 출력
    if (now - lastSerialPrintAt >= SERIAL_PRINT_INTERVAL_MS) {
        lastSerialPrintAt = now;
        printStatus(externalDistanceCm, internalDistanceCm);
    }

    // LCD 업데이트
    if (now - lastLcdUpdateAt >= LCD_UPDATE_INTERVAL_MS) {
        lastLcdUpdateAt = now;
        updateLcd(externalDistanceCm, internalDistanceCm);
    }

    // Wi-Fi가 끊어지면 재연결 (30초마다 한 번만 시도, 매 루프 멈춤 방지)
    if (WiFi.status() != WL_CONNECTED && now - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS) {
        lastWifiRetryAt = now;
        connectWiFi();
    }

    // 핸드폰에서 보낸 명령 수신 (원격으로 뚜껑 열기)
    receiveUdpCommand(now);

    // UDP로 핸드폰에 상태 전송 (모터 동작 시간 + 쓰레기 잔량)
    if (now - lastUdpSendAt >= UDP_SEND_INTERVAL_MS) {
        lastUdpSendAt = now;
        sendUdpStatus();
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
    // 닫힘(CLOSE_ANGLE) → 열림(OPEN_ANGLE)까지 1도씩 천천히 이동
    for (int angle = CLOSE_ANGLE; angle >= OPEN_ANGLE; angle -= 1) {
        lidServo.write(angle);
        delay(SERVO_STEP_DELAY_MS);
    }
    lidState = OPENED;
    openedAt = now;
    lastOpenTimeSec = now / 1000; // 모터가 움직인 시각 기록
    stableDetectCount = 0;
    Serial.println(">>> Lid OPENED");
}

void closeLid() {
    // 열림(OPEN_ANGLE) → 닫힘(CLOSE_ANGLE)까지 1도씩 천천히 이동
    for (int angle = OPEN_ANGLE; angle <= CLOSE_ANGLE; angle += 1) {
        lidServo.write(angle);
        delay(SERVO_STEP_DELAY_MS);
    }
    lidState = CLOSED;
    stableDetectCount = 0;
    Serial.println(">>> Lid CLOSED");
}

void updateTrashLevel(float internalDistanceCm) {
    if (internalDistanceCm < 0) {
        trashLevel = EMPTY;
        trashFillPercent = 0;
        return;
    }

    // 잔량 퍼센트 계산 (거리가 가까울수록 가득 참)
    float fill = (TRASH_CAN_HEIGHT_CM - internalDistanceCm) / TRASH_CAN_HEIGHT_CM * 100.0;
    if (fill < 0) fill = 0;
    if (fill > 100) fill = 100;
    trashFillPercent = (int)fill;

    if (internalDistanceCm <= FULL_THRESHOLD_CM) {
        trashLevel = FULL;
    } else if (internalDistanceCm <= YELLOW_THRESHOLD_CM) {
        trashLevel = NORMAL;
    } else {
        trashLevel = EMPTY;
    }
}

void readLightSensor() {
    lightAnalogValue = analogRead(LIGHT_AO_PIN);
    lightDigitalValue = digitalRead(LIGHT_DO_PIN);
}

void printStatus(float externalDistanceCm, float internalDistanceCm) {
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.print("s] ");

    Serial.print("Hand: ");
    if (externalDistanceCm < 0) {
        Serial.print("timeout");
    } else {
        Serial.print(externalDistanceCm);
        Serial.print("cm");
    }

    Serial.print(" | Lid: ");
    Serial.print(lidState == OPENED ? "OPEN" : "CLOSED");

    Serial.print(" | Trash: ");
    if (internalDistanceCm < 0) {
        Serial.print("timeout");
    } else {
        Serial.print(internalDistanceCm);
        Serial.print("cm");
    }

    Serial.print(" | Level: ");
    switch (trashLevel) {
        case EMPTY:  Serial.print("EMPTY");  break;
        case NORMAL: Serial.print("NORMAL"); break;
        case FULL:   Serial.print("FULL");   break;
    }

    Serial.print(" | Light: ");
    Serial.print(lightAnalogValue);
    Serial.print(lightDigitalValue ? " (dark)" : " (bright)");

    Serial.println();
}

void updateLcd(float externalDistanceCm, float internalDistanceCm) {
    lcd.clear();

    // 1줄: 뚜껑 상태 + 조도
    lcd.setCursor(0, 0);
    lcd.print(lidState == OPENED ? "OPEN" : "CLOS");

    lcd.setCursor(5, 0);
    if (externalDistanceCm > 0 && externalDistanceCm <= DETECT_DISTANCE_CM) {
        lcd.print("H:");
        lcd.print((int)externalDistanceCm);
        lcd.print("cm");
    } else {
        lcd.print("H:--");
    }

    lcd.setCursor(12, 0);
    lcd.print("L:");
    lcd.print(lightAnalogValue / 100);

    // 2줄: 쓰레기 잔량
    lcd.setCursor(0, 1);
    switch (trashLevel) {
        case EMPTY:
            lcd.print("Trash: OK");
            break;
        case NORMAL:
            lcd.print("Trash:");
            if (internalDistanceCm > 0) {
                lcd.print((int)internalDistanceCm);
                lcd.print("cm");
            }
            lcd.setCursor(12, 1);
            lcd.print("MID");
            break;
        case FULL:
            lcd.print("Trash:FULL!");
            break;
    }
}

// ==============================
// Wi-Fi 연결
// ==============================
void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    Serial.printf("[WiFi] %s 연결 시도\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // 최대 8초만 시도하고, 연결 안 되면 포기하고 진행 (쓰레기통은 계속 작동)
    unsigned long connectionStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - connectionStart < 8000) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] 연결 완료");
        Serial.print("[WiFi] ESP32 IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] 핸드폰(게이트웨이) IP: ");
        Serial.print(WiFi.gatewayIP());
        Serial.print(":");
        Serial.println(PHONE_PORT);
    } else {
        Serial.println("[WiFi] 연결 실패 - 오프라인 모드로 진행");
    }
}

// ==============================
// UDP로 핸드폰에 상태 전송
// (모터 동작 시간 + 쓰레기 잔량만)
// ==============================
void sendUdpStatus() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[UDP] Wi-Fi 연결 안 됨");
        return;
    }

    udpSequence++;

    const char* levelStr = "EMPTY";
    if (trashLevel == NORMAL) levelStr = "NORMAL";
    else if (trashLevel == FULL) levelStr = "FULL";

    // 전송할 JSON 생성 (필요한 정보만)
    String json = "{";
    json += "\"deviceId\":\"";
    json += DEVICE_ID;
    json += "\",";
    json += "\"lastOpenTimeSec\":"; // 모터가 마지막으로 열린 시각 (초)
    json += lastOpenTimeSec;
    json += ",";
    json += "\"trashLevel\":\"";    // 잔량 상태 (EMPTY/NORMAL/FULL)
    json += levelStr;
    json += "\",";
    json += "\"trashFillPercent\":"; // 잔량 퍼센트 (0~100)
    json += trashFillPercent;
    json += "}";

    // 핸드폰(게이트웨이) IP로 UDP 전송
    IPAddress phoneIp = WiFi.gatewayIP();
    if (!udp.beginPacket(phoneIp, PHONE_PORT)) {
        Serial.println("[UDP] 패킷 생성 실패");
        return;
    }

    udp.write(reinterpret_cast<const uint8_t*>(json.c_str()), json.length());

    if (udp.endPacket() == 1) {
        Serial.print("[UDP] 전송 완료: ");
        Serial.println(json);
    } else {
        Serial.println("[UDP] 전송 실패");
    }
}

// ==============================
// 핸드폰에서 보낸 명령 수신
// "OPEN" 문자열을 받으면 원격으로 뚜껑 열기
// ==============================
void receiveUdpCommand(unsigned long now) {
    int packetSize = udp.parsePacket();
    if (packetSize <= 0) {
        return;
    }

    char buffer[32];
    int len = udp.read(buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return;
    }
    buffer[len] = '\0';

    Serial.print("[UDP] 명령 수신: ");
    Serial.println(buffer);

    // "OPEN" 명령 → 뚜껑 열기
    if (strncmp(buffer, "OPEN", 4) == 0) {
        if (lidState == CLOSED) {
            openLid(now);
            Serial.println(">>> 핸드폰 명령으로 뚜껑 열림");
        } else {
            // 이미 열려 있으면 열림 시간 연장
            openedAt = now;
        }
    }
}