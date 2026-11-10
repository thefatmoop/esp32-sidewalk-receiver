# Third-party notices

## TinyGPS++ 1.0.3

The optional GNSS diagnostics use [TinyGPS++ by Mikal Hart](https://github.com/mikalhart/TinyGPSPlus). The retained source is under `esp32_s2Compatible_loraTest_RX_baseStationForMap/lib/TinyGPSPlus-1.0.3`.

Copyright (C) 2008–2013 Mikal Hart. The library source grants redistribution under GNU LGPL version 2.1 or later. Original copyright notices are retained. The LGPL and referenced GPL texts accompany the library as `COPYING.LESSER` and `COPYING`.

## Build dependencies

PlatformIO downloads Espressif's Arduino framework and toolchain during the build. Their upstream licenses remain applicable. The public release contains the project's source and the retained TinyGPS++ source.

## Protocol references

Amazon's public Sidewalk specification supplies the frame and hopping descriptions. Silicon Labs AN633 supplies independent PHY validation examples. These references are acknowledged in source comments and the README. The project implements a receiver and parser independently.
