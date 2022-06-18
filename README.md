GPS Clock Display Design
========================

14x6: (5110 LCD)
┌──────────────┐
│2022-06-15 WED│
│T22:17:15+0800│
│T+24.5 A=0170M│
│▶10.0km C=120°│
│GNSS 3D PL04hf│
│E=2.5m S16 [_]│
└──────────────┘

Time format: %Y%m%dT%H%M%S%z
T -> Temperature
A: Altitude
▶: Speed in kph (km/h)
C: Course
GNSS / TCXO: Time source
3D / 2D / NA -> Fix type
PL04HF / PL04??: Maidenhead Grid Locator
E: Fix Error Range, format: %uM
S16: Satellite count: S%u
[_] / [#]: GPIO Status

BOM:
* Nokia 5110 LCD Display
* ESP32
* Any GNSS Receiver
* DS3231
