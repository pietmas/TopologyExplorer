from build123d import *


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
