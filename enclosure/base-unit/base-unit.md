# Printalyzer Enclosure Base Unit

## Introduction

This directory contains all the assets necessary to produce the physical
base unit enclosure for the Printalyzer. This document explains how to use
those assets, and ultimately how to assemble the enclosure.

## Electronics

### Main Board

The assets for the main circuit board are contained within the "main-board"
project under the top-level "hardware" directory. This board is installed
into the "cover" part of the main enclosure.

### Power Board

The assets for the main circuit board are contained within the "power-board"
project under the top-level "hardware" directory. This board is installed
into the "base" part of the main enclosure.

## Enclosures

### Main Enclosure

#### Enclosure components

The main enclosure is designed for sheet metal fabrication as two components.
The "base" is the bottom half where the power board is installed, and the
"cover" is the top half where the main board is installed.
Basic mechanical design files are provided, but the artwork is not published.

The provided files are as follows:
* `base-unit.step` - Complete enclosure model
* `base-flat.dxf` - Flat pattern for the "base"
* `cover-flat.dxf` - Flat pattern for the "cover"
* `design-drawings.pdf` - Basic design drawings

Base fabrication notes:
* Material: Galvanized sheet SGCC, 1.2mm thickness
* Contains 2 blind threaded standoffs, M3x0.5, 8mm length (PEM BSO-M3-8ZI)
* Contains 6 self-clinching nuts, M4x0.7 (PEM S-M4-1ZI)
* All blind standoffs must be inserted before powder coating, so that they are difficult to see from the outside of the finished product.
* Powder coat exterior surfaces only. Do not powder coat interior surfaces or the fold-up tabs with the self-clinching nuts.

Cover fabrication notes:
* Material: Galvanized sheet SGCC, 1.2mm thickness
* Contains 4 blind threaded standoffs, M3x0.5, 10mm length (PEM BSO-M3-10ZI)
* All blind standoffs must be inserted before powder coating, so they are
  difficult to see on the finished product.
* Inside of front edges, where the folds meet, need to be tack welded.
* Powder coat exterior surfaces only.  Do not powder coat interior surfaces.

#### Mounting hardware

Mounting hardware for the main enclosure consists of the following
components from [McMaster-Carr](https://www.mcmaster.com/), unless otherwise
specified. It should be possible to substitute hardware from other vendors as
long as it has the same specifications.


* Mounting "main-board" PCB to enclosure cover
  * **92005A118 (Qty. 4)** - Zinc-Plated Steel Pan Head Screw,
    M3 x 0.5mm Thread, 8mm Long
  * **98688A112 (Qty. 4)** - General Purpose Zinc-Plated Steel Washer for M3 Screw
  * **93925A417 (Qty. 4)** - Zinc-Plated Internal-Tooth Washer for M3 Screw

* Mounting OLED display module to "main-board" PCB
  * **90657A106 (Qty. 4)** - Zinc-Plated Steel Narrow Cheese Head Slotted Screw,
    M3 x 0.5 mm Thread, 10 mm Long, 4 mm Head Diameter
  * **93657A203 (Qty. 4)** - Nylon Unthreaded Spacer,
    3 mm Long, for M3 Screw Size
  * **95225A315 (Qty. 4)** - Electrical-Insulating Hard Fiber Washer,
    for M3 Screw Size, 3.2 mm ID, 7 mm OD
  * **90576A102 (Qty. 4)** - Medium-Strength Steel Nylon-Insert Locknut,
    M3 x 0.5 mm Thread, 4 mm High

* Mounting "power-board" PCB to enclosure base
  * **92005A118 (Qty. 2)** - Zinc-Plated Steel Pan Head Screw,
    M3 x 0.5mm Thread, 8mm Long
  * **98688A112 (Qty. 2)** - General Purpose Zinc-Plated Steel Washer for M3 Screw
  * **93925A417 (Qty. 2)** - Zinc-Plated Internal-Tooth Washer for M3 Screw

* Mounting rear panel power connectors to "power board" PCB
  * **92005A120 (Qty. 6)** - Zinc-Plated Steel Pan Head Screw,
    M3 x 0.5mm Thread, 10mm Long
  * **95225A315 (Qty. 6)** - Electrical-Insulating Hard Fiber Washer,
    for M3 Screw Size, 3.2 mm ID, 7 mm OD
  * **90576A102 (Qty. 6)** - Medium-Strength Steel Nylon-Insert Locknut,
    M3 x 0.5 mm Thread, 4 mm High

* Mounting rear panel power and XLR connectors to enclosure base
  * **95836A209 (Qty. 6)** - Black-Oxide 18-8 Stainless Steel Pan Head Phillips Screw,
    M3 x 0.50 mm Thread, 10mm Long
  * **90576A102 (Qty. 6)** - Medium-Strength Steel Nylon-Insert Locknut,
    M3 x 0.5 mm Thread, 4 mm High
  * **A-SCREW-1-8 (Qty. 2)** - Black self-tapping Plastite screw
    (sold by Neutrik via electronics distributors)

#### Non-PCB Electronic Components
These electronic components are not included in the PCB's bill-of-materials,
but are ordered from the same kinds of vendors and are necessary to assemble
a complete device.

* Blackout switch
  * **Marquardt 1911.1102**
  * Rocker Switches ROCKER ON/OFF SWITCH SPST
* Power switch
  * **Marquardt 1801.1148**
  * Rocker Switches ROCKER ON/OFF SWITCH SPST
* Footswitch jack
  * **CUI MJ-3502**
  * Phono Connector 3.5mm Mono Jack
* Footswitch jack retaining ring
  * **Befaco Bananuts Black**
* Main fuse
  * **Littelfuse 0217005.MXBP**
  * Cylindrical cartridge fuse, 5x20mm
  * Fast-blow, rated for 5A @ 250V
* Graphic OLED display module
  * **BuyDisplay ER-OLEDM032-1Y**
  * 3.2" diagonal, 256x64 pixels, 4-wire SPI
  * Choose the version without the pin header
* 01x08 pin header, 2.54mm pitch
  * **Omron XG8T-1631**
  * Used to connect between the display module and the main PCB
  * Shorter than a standard pin header
* Key parallel guide
  * **Marquardt 190.059.023 (Qty. 2)**
  * Used as part of mounting the switches that have double-sized key caps

#### Key caps
For the key caps, use the following
[Marquardt](https://www.marquardt-switches.com/) parts:
* **825.000._xxx_ (Qty. 8)** - Square key cap, without LED window, 19mm pitch
* **844.000._xxx_ (Qty. 2)** - Double key cap, without LED window, 19mm pitch

The **_xxx_** portion of the part number can be any of the following:
* **.011** - Anthracite (Black)
* **.021** - Dark grey
* **.031** - Grey

_Note: Darker colors may look better when displaying the device under normal
room lighting, but lighter colors may be easier to see under actual
darkroom conditions._

For the encoder, use a knob designed for a 6mm D shaft. It is also preferable
to use a knob that has no direction marking.
The following choices have been tested so far:
* mxuteuk Black Aluminum Alloy Potentiometer Control Knob\
  Screw Type 17 x 16mm\
  Part # **KNOB-04-BK**
* mxuteuk Silver Aluminum Alloy Potentiometer Control Knob\
  Screw Type 17 x 16mm\
  Part # **KNOB-04-SR**

_For quantity orders, this same knob can be found from other vendors at
a lower price._

#### Internal wiring

* Connecting Blackout switch and Footswitch connector to the "main-board" PCB
  * **JST PHR-2 (Qty. 2)** - JST PH housing receptacle (2-pin)
  * **JSR ASPHSPH24K102 (Qty. 2)** - JST PH 4" jumper wire (cut each in half)
* Connecting "power-board" and "main-board" PCBs for power and relay
  * **JST PHR-4 (Qty. 2)** - JST PH housing receptacle (4-pin)
  * **JST ASPHSPH24K102 (Qty. 4)** - JST PH 4" jumper wire
* Connecting "power-board" and "main-board" PCBs for DMX connector
  * **JST A06SR06SR30K102A (Qty. 1)** - JST SR 4" 6-position reversed jumper wire
* Connecting "power-board" to the power switch
  * **Molex 46992-0210** - Mini-Fit Jr. Receptacle Housing
  * **Molex 215340-1122** - Mini-Fit Jr. Pre-Crimped Lead (cut in half)
  * **TE 3-520125-2 (Qty. 2)** - TE 187 Ultra-Fast Quick Disconnect Receptacle

_If there is not good electrical conductivity between the boards and the enclosure, and between the two halves of the enclosure, then a ground wire with eyelet terminals must be connected between a mounting hole on each board._

#### Other

* Rubylith-type material
  * This is a filter that is installed into the underside of the enclosure top,
    above the display module, to make the output spectrum of the display
    paper-safe.
  * The current preferred material for this purpose is Rosco E-Color #106.
* Thin transparent material to sandwich the rubylith between, for mounting
  and scratch protection.
