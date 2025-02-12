#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// UUIDs for the service and characteristic
static BLEUUID serviceUUID("12d985a1-b718-44aa-a1d3-44147b464ff8");
static BLEUUID charUUID("121d6534-3a65-4486-bd58-908825881214");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;

// Variables to track received distance data
float currentDistance = 0.0;
float maxDistance = -INFINITY;
float minDistance = INFINITY;
int dataCount = 0;

// Function to handle received BLE notifications
static void notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{

  // Convert received data from byte array to string
  String receivedString = "";
  for (int i = 0; i < length; i++)
  {
    receivedString += (char)pData[i];
  }

  // Convert string to float
  float receivedDistance = receivedString.toFloat();

  // Ignore invalid readings
  if (receivedDistance <= 0)
  {
    Serial.println("Received invalid data, skipping...");
    return;
  }

  // Update tracking variables
  currentDistance = receivedDistance;
  if (receivedDistance > maxDistance)
    maxDistance = receivedDistance;
  if (receivedDistance < minDistance)
    minDistance = receivedDistance;

  // Increment received data count
  dataCount++;

  // Print the received, max, and min distances
  Serial.print("Current Distance: ");
  Serial.print(currentDistance);
  Serial.print(" cm | Max Distance: ");
  Serial.print(maxDistance);
  Serial.print(" cm | Min Distance: ");
  Serial.println(minDistance);
}

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient) {}

  void onDisconnect(BLEClient *pclient)
  {
    connected = false;
    Serial.println("Disconnected");
  }
};

// Function to connect to the BLE Server
bool connectToServer()
{
  Serial.print("Connecting to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (pClient->connect(myDevice))
  {
    Serial.println("Connected to server");
  }
  else
  {
    Serial.println("Failed to connect to server");
    return false;
  }

  // Get the service from the server
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.println("Failed to find service UUID");
    pClient->disconnect();
    return false;
  }
  Serial.println("Found service");

  // Get the characteristic from the service
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr)
  {
    Serial.println("Failed to find characteristic UUID");
    pClient->disconnect();
    return false;
  }
  Serial.println("Found characteristic");

  // Subscribe to notifications
  if (pRemoteCharacteristic->canNotify())
  {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connected = true;
  return true;
}

// BLE Scanning callback
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.print("Found device: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    }
  }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE Client...");

  BLEDevice::init("");

  // Start BLE scanning
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop()
{
  if (doConnect)
  {
    if (connectToServer())
    {
      Serial.println("Connected to BLE Server.");
    }
    else
    {
      Serial.println("Failed to connect to server.");
    }
    doConnect = false;
  }

  if (connected)
  {
    delay(1000); // Avoid overwhelming the BLE connection
  }
  else if (doScan)
  {
    BLEDevice::getScan()->start(0);
  }
}
