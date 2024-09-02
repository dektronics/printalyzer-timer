from kikit.units import mm
from kikit.common import resolveAnchor
from kikit.defs import EDA_TEXT_HJUSTIFY_T, EDA_TEXT_VJUSTIFY_T, Layer
import pcbnew
import sys

def kikitPostprocess(panel, arg):
    # Add revision information to bottom rail
    position = resolveAnchor("mb")(panel.boardSubstrate.boundingBox())
    position.y -= int(2.5 * mm)
    panel.addText("Rev: ${REVISION} [${ISSUE_DATE}]", position, hJustify=EDA_TEXT_HJUSTIFY_T.GR_TEXT_HJUSTIFY_CENTER)

    # Remove all dimension marks from the board
    for element in panel.board.GetDrawings():
        if isinstance(element, pcbnew.PCB_DIMENSION_BASE) and element.GetLayerName() == "User.Comments":
            panel.board.Remove(element)

    # Prepare panel dimension anchors
    minx, miny, maxx, maxy = panel.panelBBox()
    tlAnchor = pcbnew.VECTOR2I(int(minx), int(miny))
    trAnchor = pcbnew.VECTOR2I(int(maxx), int(miny))
    blAnchor = pcbnew.VECTOR2I(int(minx), int(maxy))
    
    # Add top panel dimension
    dim = pcbnew.PCB_DIM_ALIGNED(panel.board)
    dim.SetStart(tlAnchor)
    dim.SetEnd(trAnchor)
    dim.SetHeight(int(-2.5 * mm));
    dim.SetUnits(pcbnew.EDA_UNITS_MILLIMETRES)
    dim.SetPrecision(pcbnew.DIM_PRECISION_X_X)
    dim.SetLayer(Layer.Cmts_User)
    panel.board.Add(dim)
    
    # Add side panel dimension
    dim = pcbnew.PCB_DIM_ALIGNED(panel.board)
    dim.SetStart(tlAnchor)
    dim.SetEnd(blAnchor)
    dim.SetHeight(int(2.5 * mm));
    dim.SetUnits(pcbnew.EDA_UNITS_MILLIMETRES)
    dim.SetPrecision(pcbnew.DIM_PRECISION_X_X)
    dim.SetLayer(Layer.Cmts_User)
    panel.board.Add(dim)
