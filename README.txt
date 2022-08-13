GPS Clock Display Design
========================

14x6: (5110 LCD)
┌──────────────┐
│2022-06-15 WED│
│T22:17:15+0800│
│T+24.5 A=0170M│
│P=0985.3 H=63%│
│>10.0kph C^120│
│GPS S12 PL04hf│
└──────────────┘

Time format: %Y%m%dT%H%M%S%z
T -> Temperature
A: Altitude
>: Speed in kph (km/h)
C^: Course
GPS / RTC: Time source
S?? / N/A -> Satellite count / No Fix
PL04HF / PL04??: Maidenhead Grid Locator

BOM:
* Nokia 5110 LCD Display
* ESP32
* Any GNSS Receiver
* DS3231
* BME280
