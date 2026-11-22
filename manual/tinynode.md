# TinyNode: the relays and sensors

TinyNode is a remote control for the node: the Arduino board, in the
TinyNode repository, that sits away from the console with four
relays, a GPS, a magnetometer, a temperature and humidity sensor and a
LoRa radio. The window talks to it over the radio. It is a control
surface only: the node does the work, and the window sends it the same
commands it already understands.

## What it needs

- The LoRa header, with its REYAX RYLR998, in PMOD0, and
  `layout = lora_i2s2` in /tinytang.ini, or `layout = lora` when no
  Pmod I2S2 is fitted.
- The node powered and in range.

## Relays

Four boxes, one per relay. The numbers follow the switched cable, not
the header:

     Relay 1   VBUS
     Relay 2   D-
     Relay 3   D+
     Relay 4   GND

Ticking or clearing a box sends the four states to the node at once.
When it confirms, the line beside Update says `relay states updated`.
If it does not, the boxes go back to the last states the node
confirmed, and the line says why.

## Sensors

Update asks the node for its status. One reply carries everything it
knows, shown in the list below: the GPS fix and position, the
magnetometer's vector and heading, the temperature and humidity, the
PPS measurement, the radio's counters and the relay states. The boxes are
set from that reply too, so Update also shows what the relays really
are.

Nothing is read when the window opens; press Update.

## When the node does not answer

Every click is a round trip over the radio, and the radio loses
messages. Each request is tried three times, waiting four seconds
each, so a missing node shows `no reply from the node` after about
twelve seconds. The desktop keeps working meanwhile. A click made while a
request is out is refused with `still waiting for the node`, not
queued.
