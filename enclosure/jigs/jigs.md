# Assembly Jigs

## Introduction

This directory contains all the assets necessary to create the various jigs
used to aid in the assembly of the various components of the Printalyzer
Enlarging Timer and its accessories.

## PCB Assembly

These jigs are designed to help hold various components in place for soldering,
fastening, or general bench work. Many of them contain places where metal dowel
pins can be inserted to help hold the boards in correct alignment.

### Main Board

* `jig-main-board-platform-1`
  * Platform to support the main board when it is being worked on. (Orientation 1)
  * Shorter platform that fits the board with the button side facing upwards, or either side if
    the display and encoder are not yet installed.
* `jig-main-board-platform-2`
  * Platform to support the main board when it is being worked on. (Orientation 2)
  * Taller platform that fits the board with the button side facing downwards,
    with extra clearance for both the display and the encoder.
* `jig-main-board-buttons`
  * Jig to hold all of the push buttons in place for soldering.
  * Does not have clearance for the display, encoder, or button caps.
* `jig-main-board-leds`
  * Jig to hold all of the panel LEDs in place for soldering.
  * Does not have clearance for the display, encoder, or button caps.

### Power Board

* `jig-power-board-1`
  * Jig to hold all of the thru-hole components in place for soldering, and to
    make sure all the rear panel connectors are correctly aligned for fastening.
  * This board is designed to have a thin piece of adhesive foam placed onto
    each of the component holding standoffs, to help provide compliant pressure
    and deal with any vertical tolerance issues.
* `jig-power-board-2`
  * Jig to hold the nuts and washers in place on the bottom side of the board.
