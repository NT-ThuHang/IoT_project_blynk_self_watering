#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define PUMP_PIN                        17      
#define PUMP_LOGIC_INVERTED             false
#define MOISTURE_SENSOR_POWER_PIN       4       

// Blynk App settings:
// Giá trị hẹn giờ bơm 
#define BLYNK_APP_PUMPTIMERVALUE_VPIN   V0    
// Nút ấn để BẬT máy bơm cho giá trị hẹn giờ ở trên  
#define BLYNK_APP_PUMPONTIMER_VPIN      V1      
// Nút BẬT / TẮT máy bơm bằng tay khi thấy giá trị độ ẩm quá thấp
#define BLYNK_APP_PUMPONOFF_VPIN        V2    
 // Hẹn giờ tưới  
#define BLYNK_APP_PUMPTIMERSTATUS_VPIN  V3  
// Trạng thái TẮT hoặc BẬt hiện tại của máy bơm.    
#define BLYNK_APP_PUMPSTATUS_VPIN       V4  
// Lượng nước hiện có trong đất    
#define BLYNK_APP_WATERINLAND_VPIN      V5 
// Giá trị độ ẩm của đất.     
#define BLYNK_APP_SOILMOISTURE_VPIN     V6      
char auth[] = "0JvY_NCFGW1widbAj111RpjJDj8HNiVs";
char ssid[] = "THONG";
char pass[] = "0379191869";

BlynkTimer      timerSystem;
bool            readSensors_flag = false;
int             pumpOnTimeDuration = 1;
int             pumpOnTimer_numTimer = -1;
const unsigned  reading_count = 10;
unsigned int    analogVals[reading_count];
unsigned int    values_avg = 0;
int soilMoisture = 0;
int soilMoisture_percent=0;

// Khởi tạo delay của BLYNK để tránh đụng độ
void Blynk_Delay(int milli) {
    int end_time = millis() + milli;
    while(millis() < end_time) {
        Blynk.run();
        Blynk.run();
        timerSystem.run();
        yield();
    }
}

// Điều khiển máy bơm và báo lại trạng thái
void setPumpPower(bool power = false) {
    if (power) {
        Serial.println("Pump powered ON");
        Blynk.virtualWrite(BLYNK_APP_PUMPSTATUS_VPIN, "ON");
        if (PUMP_LOGIC_INVERTED) digitalWrite(PUMP_PIN, HIGH);
        else digitalWrite(PUMP_PIN, LOW);
    }
    else {
        Serial.println("Pump powered OFF");
        Blynk.virtualWrite(BLYNK_APP_PUMPSTATUS_VPIN, "OFF");
        if (PUMP_LOGIC_INVERTED) digitalWrite(PUMP_PIN, LOW);
        else digitalWrite(PUMP_PIN, HIGH);
    }
}

//Thiết lập thời gian tưới(Cụ thể là tưới trong bao lâu)
void setPumpTimerDuration(int duration) {
    if (duration < 1) duration = 1;
    pumpOnTimeDuration = duration;
    Blynk.virtualWrite(BLYNK_APP_PUMPTIMERVALUE_VPIN , pumpOnTimeDuration);
    Blynk.virtualWrite(BLYNK_APP_PUMPTIMERSTATUS_VPIN, pumpOnTimeDuration);
    Serial.printf("Pump duration set to %u seconds\r\n", pumpOnTimeDuration);
}

//Khi đã hết thời gian
void pumpTimerTimeout() {
    Serial.println("Timer expired");
    Blynk.virtualWrite(BLYNK_APP_PUMPONTIMER_VPIN, 0);
    timerSystem.disable(pumpOnTimer_numTimer);
    timerSystem.deleteTimer(pumpOnTimer_numTimer);
    pumpOnTimer_numTimer = -1;
    setPumpPower(false);
}

// Được gọi mỗi khi thay đổi thời gian hoạt động của máy bơm nước
// thông qua ứng dụng (ví dụ: có thể được gắn với thanh trượt):
BLYNK_WRITE(BLYNK_APP_PUMPTIMERVALUE_VPIN) {
    setPumpTimerDuration(param.asInt());
}

// Được gọi mỗi khinhấn nút nhấn để BẬT máy bơm trong
// khoảng thời gian được chỉ định bởi 'pumpOnTimeDuration':
BLYNK_WRITE(BLYNK_APP_PUMPONTIMER_VPIN) {
    if (param.asInt() == 1) {
        if (pumpOnTimer_numTimer < 0) {
            Serial.printf("Starting timer of %u seconds\r\n", pumpOnTimeDuration);
            setPumpPower(true);
            pumpOnTimer_numTimer = timerSystem.setTimeout(pumpOnTimeDuration*1000L, pumpTimerTimeout);
        }
        else {
            Serial.println("Pump was already on a timer. Ending timer...");
            pumpTimerTimeout();
        }
    }
}

// được gọi mỗi khi nhấn BẬT TẮT máy bớm
BLYNK_WRITE(BLYNK_APP_PUMPONOFF_VPIN) {
    if (param.asInt() == 0) setPumpPower(false);
    else setPumpPower(true);
}

// đo độ ẩm
int getSoilMoisture() {
    // Turn the sensor ON:
    digitalWrite(MOISTURE_SENSOR_POWER_PIN, HIGH);
    Blynk_Delay(100);
    
    for (int counter = 0; counter < reading_count; counter++) {
        analogVals[reading_count] = analogRead(A0);
        Blynk_Delay(100);
        values_avg = (values_avg + analogVals[reading_count]);
    }
    values_avg = values_avg/reading_count;

    // Turn the sensor OFF:
    digitalWrite(MOISTURE_SENSOR_POWER_PIN, LOW);
    return values_avg;
}

void readSensors() {
    readSensors_flag = true;
}

void setup() {
    Serial.begin(9600);
    Serial.println("Preparing...");
    
    //Thiết lập pin
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(MOISTURE_SENSOR_POWER_PIN, OUTPUT);

    // Bắt đầu kết nối với hệ thống WiFi và Blynk:
    Blynk.begin(auth, ssid, pass);
    
    // Trạng thái đầu
    setPumpTimerDuration(3);
    setPumpPower(false);
    Blynk.virtualWrite(BLYNK_APP_PUMPONTIMER_VPIN, 0);
    
    timerSystem.setInterval(6000L, readSensors);
    
    Serial.println("Ready...");
}

void loop() {
    Blynk.run();
    timerSystem.run();
    // Đọc các cảm biến tại đây ngay sau khi cờ được đặt. Đọc tất cả các cảm biến và báo cáo lại ứng dụng Blynk
    if (readSensors_flag) {
        soilMoisture = getSoilMoisture();
        soilMoisture_percent = map(soilMoisture, 4095, 0, 0, 100);
        Blynk.virtualWrite(BLYNK_APP_WATERINLAND_VPIN, soilMoisture_percent);
        // Nếu độ âm quá thấp thì phải tưới
        if (soilMoisture_percent<10)
        {
          setPumpPower(true);
          delay(5000);
          setPumpPower(false);
        }
        //Nếu độ ẩm ở mức báo động thì gửi thông báo
        if (soilMoisture_percent>=10 && soilMoisture_percent<20)
        {
          Blynk.notify("I want to drink :( ");
        }
        Serial.print("Read soil moisture: ");
        Serial.println(soilMoisture);
        Blynk.virtualWrite(BLYNK_APP_SOILMOISTURE_VPIN, soilMoisture);
        readSensors_flag = false;
    }
}
