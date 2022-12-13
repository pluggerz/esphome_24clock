#include <Arduino.h>
#include <SoftwareSerial.h>

#include "pins.h"

SoftwareSerial oneWireSerial(SYNC_IN_PIN, SYNC_OUT_PIN);

void OneWireProtocol::write(int value)
{
    oneWireSerial.write(value);
}