import topoexplorer as te
from bridge import to_brep_string
import pytest
from parts import block, filleted_block, plate_with_holes


def test_block_counts():
    g = te.analyze(to_brep_string(block()))
    assert (g.face_count, g.edge_count, g.vertex_count) == (6, 12, 8)
    assert len(g.arcs) == 12


def test_filleted_block_has_cylinders():
    g = te.analyze(to_brep_string(filleted_block()))
    types = [f.surface_type for f in g.faces]
    assert types.count("Cylinder") == 12
    assert "Sphere" in types


def test_block_euler():
    assert te.analyze(to_brep_string(block())).euler == 2


@pytest.mark.parametrize("n", [1, 3, 4])
def test_plate_with_holes_count(n):
    g = te.analyze(to_brep_string(plate_with_holes(n=n)))
    types = [f.surface_type for f in g.faces]
    assert types.count("Cylinder") == n


def test_plate_with_holes_euler():
    # genus 4 solid: 2 - 2*4
    g = te.analyze(to_brep_string(plate_with_holes(n=4)))
    assert g.euler == -6
