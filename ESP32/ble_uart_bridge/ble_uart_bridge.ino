#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <HardwareSerial.h>

String rxValue;        // RX STRING VALUE received from BLE

HardwareSerial UART(2);

BLEServer           * pServer            = NULL;
BLECharacteristic   * pTxCharacteristic  = NULL;
BLEService          * pService           = NULL;
bool                  deviceConnected    = false;
bool                  oldDeviceConnected = false;

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // Nordic UART service UUID
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

int countBytes = 0;

uint8_t RXbuffer[256];
uint8_t TXbuffer[256];

class MyServerCallbacks: public BLEServerCallbacks
{
    void onConnect(BLEServer* pServer)
    {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer)
    {
        deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        Serial.println("Got new value from BLE");
        rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0)
        {
            for (int i = 0; i < rxValue.length(); i++)
            {
                TXbuffer[i] = rxValue[i];
            }
            UART.write(TXbuffer, rxValue.length());
        }
    }
};

void setBLE()
{
    Serial.println("Init BLE...");
    Serial.println(SERVICE_UUID);

    deviceConnected    = false;
    oldDeviceConnected = false;

    // Init BLE stack (BLEDevice is a static class — no instance needed)
    BLEDevice::init("DIYDragy");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    pService = pServer->createService(SERVICE_UUID);

    // TX characteristic (ESP32 -> phone)
    pTxCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_TX,
                            BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());

    // RX characteristic (phone -> ESP32)
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                            CHARACTERISTIC_UUID_RX,
                            BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Advertise the service UUID so apps filtering by Nordic UART can find us
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);   // helps with iPhone connections
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising as DIYDragy");
}

// Called once at startup
void setup()
{
    Serial.begin(115200);
    UART.begin(9600, SERIAL_8N1, 16, 17);
    setBLE();
}

// Main loop
void loop()
{
    if (deviceConnected)
    {
        countBytes = UART.available();
        while (countBytes >= 20)
        {
            UART.readBytes(RXbuffer, 20);
            pTxCharacteristic->setValue(RXbuffer, 20);   // tx to BLE
            pTxCharacteristic->notify();
            countBytes -= 20;
        }
        if (countBytes > 0) // rx from UART
        {
            UART.readBytes(RXbuffer, countBytes);
            pTxCharacteristic->setValue(RXbuffer, countBytes);   // tx to BLE
            pTxCharacteristic->notify();
        }
    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected)
    {
        Serial.println("DISCONNECT!");
        delay(500);
        pServer->startAdvertising(); // restart advertising
        oldDeviceConnected = deviceConnected;
    }

    // connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        Serial.println("CONNECTED!");
        oldDeviceConnected = deviceConnected;
    }
}