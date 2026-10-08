from build123d import *
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse


def block():
    with BuildPart() as p:
        Box(60, 40, 20)
    return p.part


def plate_with_holes(n=4, w=80, d=50, t=8, hole_r=4):
    with BuildPart() as p:
        Box(w, d, t)
        cols, rows = (n // 2, 2) if n % 2 == 0 else (n, 1)
        with GridLocations(w / (cols + 1), d / 3, cols, rows):
            Hole(hole_r)
    return p.part


def filleted_block(r=3):
    with BuildPart() as p:
        Box(60, 40, 20)
        fillet(p.edges(), radius=r)
    return p.part


def l_block():
    with BuildPart() as p:
        Box(60, 40, 20)
        with Locations((15, 0, 5)):
            Box(30, 40, 10, mode=Mode.SUBTRACT)
    return p.part


def block_with_blind_hole(r=5, depth=8):
    with BuildPart() as p:
        Box(60, 40, 20)
        with Locations(p.faces().sort_by(Axis.Z)[-1]):
            Hole(r, depth)
    return p.part


def pocketed_block(depth=8):
    with BuildPart() as p:
        Box(60, 40, 20)
        with BuildSketch(p.faces().sort_by(Axis.Z)[-1]):
            Rectangle(30, 20)
        extrude(amount=-depth, mode=Mode.SUBTRACT)
    return p.part


def block_with_boss(round_boss=False, height=8):
    with BuildPart() as p:
        Box(60, 40, 10)
        with BuildSketch(p.faces().sort_by(Axis.Z)[-1]):
            if round_boss:
                Circle(6)
            else:
                Rectangle(20, 15)
        extrude(amount=height)
    return p.part


def filleted_bracket(inner_r=3, outer_r=5):
    with BuildPart() as p:
        insert(l_block())
        along_y = p.edges().filter_by(Axis.Y)
        inner = [e for e in along_y if abs(e.center().X) < 1e-6 and abs(e.center().Z) < 1e-6]
        fillet(inner, radius=inner_r)
        along_y = p.edges().filter_by(Axis.Y)
        outer = [e for e in along_y if abs(e.center().X + 30) < 1e-6 and abs(e.center().Z - 10) < 1e-6]
        fillet(outer, radius=outer_r)
    return p.part


def split_hole_plate():
    a, b = plate_with_holes(1).split(Plane.XZ, keep=Keep.BOTH)
    return Compound(BRepAlgoAPI_Fuse(a.wrapped, b.wrapped).Shape())
