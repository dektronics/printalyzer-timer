# Printalyzer Enclosure Models

## Introduction

This directory contains all the assets necessary to produce the physical
enclosures for the Printalyzer. This document explains how to use those
assets, and ultimately how to assemble the enclosure.

## Electronics

### Main Board

The assets for the main circuit board are contained within the "main-board"
project under the top-level "hardware" directory. This board is installed
into the main enclosure.

### Meter Probe

The assets for the meter probe circuit board are contained within the
"meter-probe" project under the top-level "hardware" directory. This board
is installed into the meter probe enclosure.

## Enclosures

### Main Enclosure

#### Enclosure components
The main enclosure was created with the design tool from
[Front Panel Express](https://www.frontpanelexpress.com/), and consists of
the following components:
* `Top.fpd` - Main panel of the device, which has the majority of the printing
  and mounting hardware.
* `Bottom.fpd` - Bottom of the device, which is relatively simple in its design.
* `Front.fpd` - Front of the device, which has holes for input ports and the
  blackout switch.
* `Back.fpd` - Back of the device, which has holes for all the power
  connections, the main fuse, and the power switch.
* **GLGP1023 (Qty. 2)** - Side Profile 2 / black, black anodized, Length: 156mm
* **GGMS1122** - Assembly kit ISP/B3.0-SW DIN 7982, black galvanized
* **GGRB3306** - Screw packet M3x6-965-SW DIN 965, black galvanized
* **GGWS0111** - Housing brackets 4 x per unit company standard, natural anodized

#### Mounting hardware
Mounting hardware for the main enclosure consists of the following
components from [McMaster-Carr](https://www.mcmaster.com/). It should be
possible to substitute hardware from other vendors as long as it has the
same dimensions.


* Mounting PCB to enclosure top
  * **92000A118 (Qty. 4)** - Passivated 18-8 Stainless Steel Pan Head Phillips Screw,
    M3 x 0.5mm Thread, 8mm Long


* Mounting IEC320 power connectors to PCB and enclosure
  * **95836A209 (Qty. 12)** - Black-Oxide 18-8 Stainless Steel Pan Head Phillips Screw,
    M3 x 0.50 mm Thread, 10mm Long
  * **90576A102 (Qty. 12)** - Medium-Strength Steel Nylon-Insert Locknut,
    M3 x 0.5 mm Thread, 4 mm High
  * **95225A315 (Qty. 6)** - Electrical-Insulating Hard Fiber Washer,
    for M3 Screw Size, 3.2 mm ID, 7 mm OD


* Mounting OLED display module to PCB
  * **92000A106 (Qty. 4)** - Passivated 18-8 Stainless Steel Pan Head Phillips Screw,
    M2.5 x 0.45mm Thread, 10mm Long
  * **93657A202 (Qty. 4)** - Nylon Unthreaded Spacer,
    4.5 mm OD, 3 mm Long, for M2.5 Screw Size
  * **95225A310 (Qty. 8)** - Electrical-Insulating Hard Fiber Washer,
    for M2.5 Screw Size, 2.7 mm ID, 6.5 mm OD
  * **90591A270 (Qty. 4)** - Zinc-Plated Steel Hex Nut,
    M2.5 x 0.45 mm Thread


#### Non-PCB Electronic Components
These electronic components are not included in the PCB's bill-of-materials,
but are ordered from the same kinds of vendors and are necessary to assemble
a complete device.

* Blackout switch
  * **Marquardt 1911.1102**
  * Rocker Switches ROCKER ON/OFF SWITCH SPST
* Power switch
  * **Marquardt 1801.1146**
  * Rocker Switches ROCKER ON/OFF SWITCH SPST
* Footswitch jack
  * **Kycon STPX-3501-3C-1**
  * Phone Connectors 3.5mm 3P STEREO JACK PNL MNTTHREADED
* Main fuse
  * Cylindrical cartridge fuse, 5x20mm
  * Fast-blow, rated for 5A @ 250V
* Internal fuse
  * Cylindrical cartridge fuse, 5x20mm
  * Slow-blow, rated for 500mA
* Internal fuse holder cover
  * **Schurter 0853.0551**
  * Fuse holder cover, transparent, for 5x20mm fuse
* Graphic OLED display module
  * **Newhaven NHD-3.12-25664UCY2**
  * 3.12" diagonal, 256x64 pixels
* 01x20 pin header, 2.54mm pitch
  * Used to connect between the display module and the main PCB
* Key parallel guide
  * **Marquardt 190.059.023 (Qty. 2)**
  * Used as part of mounting the switches that have double-sized key caps

#### Key caps
For the key caps, use the following
[Marquardt](https://www.marquardt-switches.com/) parts:
* **825.000._xxx_ (Qty. 4)** - Square key cap, without LED window, 19mm pitch
* **828.000._xxx_ (Qty. 4)** - Square key cap, with LED window, 19mm pitch
* **844.000._xxx_ (Qty. 2)** - Double key cap, without LED window, 19mm pitch

The **_xxx_** portion of the part number can be any of the following:
* **.011** - Anthracite (Black)
* **.021** - Dark grey
* **.031** - Grey

_Note: Darker colors may look better when displaying the device in normal
room lighting, but lighter colors may be easier to see under actual
darkroom conditions._

For the encoder, use a knob designed for a 6mm D shaft. It is also preferable
to use a knob that has no direction marking.
The following two choices have been tested so far:
* Eagle Plastic Devices\
  16mm D-shaft\
  Part # **450-BA600**
* mxuteuk Black Aluminum Alloy Potentiometer Control Knob\
  Screw Type 17 x 16mm\
  Part # **KNOB-04-BK**

#### Internal wiring

* Wire assemblies with JST PH 2-pin female connectors (Qty. 2) - Used to
  connect the Blackout switch and the Footswitch connector to the main PCB.
* 16 AWG stranded wire - Used to connect the power switch to the main PCB.
* Insulated female spade connectors (Qty. 2) - Used to connect the power switch wires
  to the actual power switch.

#### Other

* Rubylith-type material
  * This is a filter that is installed into the underside of the enclosure top,
    above the display module, to make the output spectrum of the display
    paper-safe.
  * The current preferred material for this purpose is Rosco Supergel 19 Fire Gel.
* Thin transparent material to sandwich the rubylith between, for mounting
  and scratch protection.

### Meter Probe

#### Enclosure components
The meter probe enclosure was created with
[Autodesk Fusion 360](https://www.autodesk.com/products/fusion-360/overview),
and consists of the following components:
* `meter-probe-top.stl` - Top of the meter probe, which contains holes for
  the trigger button and the light sensor.
* `meter-probe-bottom.stl` - Bottom of the meter probe, which contains
  mounting points for the PCB and holes for screws that hold the enclosure
  together.

#### Mounting hardware
Mounting hardware for the meter probe enclosure consists of the following
components from [McMaster-Carr](https://www.mcmaster.com/). It should be
possible to substitute hardware from other vendors as long as it has the
same dimensions.
* **94180A321 (Qty. 7)** - Tapered Heat-Set Insert for Plastic,
  M2.5 x 0.45 mm Thread Size, 3.4 mm Installed Length 
* **95836A524 (Qty. 7)** - Black-Oxide 18-8 Stainless Steel Pan Head Phillips Screw,
  M2.5 x 0.45 mm Thread, 5mm Long
* **9600K38 (Qty. 1)** - SBR Rubber Grommet
  for 9/32" Hole Diameter and 3/32" Material Thickness\
  (_Note: This grommet is slightly undersized and fits the cable rather tightly._)

#### Key caps
For the cap on top of the trigger button, choose one of the following
[Marquardt](https://www.marquardt-switches.com/) parts:
* **827.100.011** - Anthracite (Black)
* **827.100.021** - Dark grey
* **827.100.031** - Grey

#### Cabling
The cable has a standard 6-pin Mini-DIN male connector on one end, and bare
wires (to be soldered to the meter probe PCB) on the other end.
This is the same connector that is used for standard PS/2 keyboard/mouse
cables, therefore the easiest way to get one of these cables is simply to
buy one of those cables and cut off the opposing connector.
