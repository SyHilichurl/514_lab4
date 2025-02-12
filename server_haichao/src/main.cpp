#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define TRIG_PIN 9  // GPIO pin for HC-SR04 Trig
#define ECHO_PIN 10 // GPIO pin for HC-SR04 Echo

#define SERVICE_UUID        "12d985a1-b718-44aa-a1d3-44147b464ff8"
#define CHARACTERISTIC_UUID "121d6534-3a65-4486-bd58-908825881214"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

const int FILTER_SIZE = 5;  // Adjust for smoother filtering
float distanceArray[FILTER_SIZE] = {20.0}; // Initialize with a valid starting value
int filterIndex = 0;
float lastValidDistance = 20.0; // Store the last good measurement

unsigned long previousMillis = 0;
const long interval = 1000;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// Function to measure distance using HC-SR04
float getDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 50000); // 50ms timeout
    float distance = (duration * 0.0343) / 2; // Convert to cm

    if (distance > 200 || distance <= 0) {
        return -1;  // Invalid measurement
    }
    return distance;
}

// Moving Average Filter (Handles -1 values properly)
float movingAverageFilter(float newValue) {
    if (newValue != -1) {
        lastValidDistance = newValue;  // Update last valid reading
    }

    distanceArray[filterIndex] = lastValidDistance; // Use last valid distance
    filterIndex = (filterIndex + 1) % FILTER_SIZE;

    float sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += distanceArray[i];
    }
    return sum / FILTER_SIZE;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    BLEDevice::init("XIAO_ESP32S3");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE Service started. Now waiting for client connection...");
}

void loop() {
    float rawDistance = getDistance();
    float filteredDistance = movingAverageFilter(rawDistance);

    Serial.print("Raw Distance: ");
    Serial.print(rawDistance);
    Serial.print(" cm, Filtered Distance: ");
    Serial.print(filteredDistance);
    Serial.println(" cm");

    // Transmit only if Filtered Distance < 30cm
    if (deviceConnected && filteredDistance > 0 && filteredDistance < 30) {
        pCharacteristic->setValue(String(filteredDistance).c_str());
        pCharacteristic->notify();
        Serial.print("BLE Notified: ");
        Serial.print(filteredDistance);
        Serial.println(" cm");
    }

    delay(1000);
}
