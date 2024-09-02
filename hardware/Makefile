# Makefile for building board artifacts

.PHONY: all clean

KIKIT := kikit

BOARDS = main-board power-board meter-probe densistick
PANELS = $(addsuffix -panel, $(BOARDS))
BOARDFILES = $(foreach board,$(BOARDS),$(addsuffix .kicad_pcb, $(addprefix $(board)/,$(board))))
PANELFILES = $(addprefix build/, $(PANELS:=.kicad_pcb))
GERBERS = $(addprefix build/, $(PANELS:=-gerber.zip))

all: $(GERBERS)

panels: $(PANELFILES)

build/main-board-panel.kicad_pcb: main-board/main-board.kicad_pcb main-board/main-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p main-board/main-board-panelize.json $< $@

build/power-board-panel.kicad_pcb: power-board/power-board.kicad_pcb power-board/power-board-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p power-board/power-board-panelize.json $< $@

build/meter-probe-panel.kicad_pcb: meter-probe/meter-probe.kicad_pcb meter-probe/meter-probe-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p meter-probe/meter-probe-panelize.json $< $@

build/densistick-panel.kicad_pcb: densistick/densistick.kicad_pcb densistick/densistick-panelize.json tools/kikit-post.py build
	$(KIKIT) panelize -p densistick/densistick-panelize.json $< $@

%-gerber: %.kicad_pcb
	kikit export gerber $< $@

%-gerber.zip: %-gerber
	zip -j $@ `find $<`

build:
	mkdir -p build

clean:
	rm -rf build
