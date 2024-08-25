# Printalyzer

![Printalyzer Logo](https://github.com/dkonigsberg/printalyzer/blob/master/docs/images/printalyzer-logo.png?raw=true)

![Main Unit Rendering](https://github.com/dkonigsberg/printalyzer/blob/master/docs/images/main-unit-photo.png?raw=true)

## Introduction

The Printalyzer is an F-Stop enlarging timer and print exposure meter.

A good overview of the project can be found in these blog posts:
* [Introducing the Printalyzer!](https://hecgeek.blogspot.com/2020/11/introducing-printalyzer.html)
* [Printalyzer Project Update](https://hecgeek.blogspot.com/2020/12/printalyzer-project-update.html)

Videos demonstrating the project are here:
* [Introducing the Printalyzer](https://youtu.be/2FPqqqVILP8)

The eventual goal of this project is to be available as a complete product available here:
* [Printalyzer Enlarging Timer](https://www.dektronics.com/printalyzer-enlarging-timer)

Note: This project is currently a work-in-progress, so expect the hardware and software to be continuously evolving.  The last functional version of this repository, using the revision A prototype hardware, was
'main-board-rev-a-186-g9bce418'.

## Project Layout

### Hardware
The "hardware" directory contains [KiCad](https://www.kicad.org/) projects
with the complete circuit schematics, bill-of-materials, and
PCB layouts.

* Main Board (main-board)
  * The main circuit board which contains all of the top panel controls and front panel ports
  * Runs off a +12V DC input, and contains regulators for all other voltage rails

* Power Board (power-board)
  * The AC power input board, which is the only part of the system that should ever touch mains AC power
  * Runs off power from an IEC320 C14 inlet, and provides output power to drive two IEC320 C13 outlets
  * Outputs the +12V DC power supply to run the rest of the system
  * Contains any connectors that need to be on the rear panel, isolating them if they do not involve mains AC power

* Printalyzer Meter Probe (meter-probe)
  * The circuit board contained within the external light meter probe peripheral
  * Connected to the main board and powered via a captive USB cable

* Printalyzer DensiStick (densistick)
  * The circuit board contained within the external reflection densitometer peripheral
  * Connected to the main board and powered via a captive USB cable

### Software
The "software" directory contains all the source code for the firmware
that runs on the hardware.

### Enclosure
The "enclosure" directory contains any CAD models and related resources
necessary to physically assemble the project.

## License
Individual directories will contain LICENSE files as needed, with relevant
details. Generally, the hardware will be CC BY-SA and the software will be
BSD 3-Clause. However, external resources may have their own individual license terms.

## Credits
