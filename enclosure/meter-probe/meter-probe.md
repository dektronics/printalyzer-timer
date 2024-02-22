# Printalyzer Enclosure Meter Probe

## Introduction

This directory contains all the assets necessary to produce the physical
meter probe enclosure for the Printalyzer. This document explains how to use
those assets, and ultimately how to assemble the enclosure.

## Electronics

### Meter Probe

The assets for the meter probe circuit board are contained within the
"meter-probe" project under the top-level "hardware" directory. This board
is installed into the meter probe enclosure.

## Enclosures

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
* `meter-probe-top-front.dxf` - Top front graphic overlay, with a matte clear
  window over the sensor hole.
* `meter-probe-top-rear.dxf` - Top rear graphic overlay, with a cutout over
  the trigger button.

#### Mounting hardware
Mounting hardware for the meter probe enclosure consists of the following
components from [McMaster-Carr](https://www.mcmaster.com/). It should be
possible to substitute hardware from other vendors as long as it has the
same dimensions.

* Mounting "meter-probe" PCB to the enclosure base
  * **90380A337 (Qty. 3)** - Phillips Rounded Head Thread-Forming Screws (M2.5 x 5mm long)
* Assembling the top and bottom of the enclosure
  * **90380A337 (Qty. 2)** - Phillips Rounded Head Thread-Forming Screws (M2.5 x 5mm long)
  * **90380A339 (Qty. 2)** - Phillips Rounded Head Thread-Forming Screws (M2.5 x 8mm long)

#### Key caps
For the cap on top of the trigger button, choose one of the following
[Marquardt](https://www.marquardt-switches.com/) parts:
* **827.100.011** - Anthracite (Black)
* **827.100.021** - Dark grey
* **827.100.031** - Grey

#### Other components
* 8mm diameter light diffuser, cut from LGT125J material with a hole punch
* 10mm diameter (0.5-1.0mm thick) UV-IR cut filter
* Captive USB mouse cable, with a rubber strain relief and a
  JST PH-compatible 5-pin internal connector.
