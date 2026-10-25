import glob
import os
import tempfile
import pytest
import topoexplorer as te
from bridge import step_to_brep_string, to_brep_string
from build123d import export_step
from parts import (ball, block, filleted_block, filleted_bracket, flat_sheet,
                   glued_blocks, lofted_pocket, plate_with_holes, ring,
                   two_blocks)

SAMPLES = "/usr/share/opencascade/data"
needs_samples = pytest.mark.skipif(not os.path.isdir(SAMPLES), reason="no occt samples")


def _graph(part):
    return te.analyze(to_brep_string(part))


def _sample(name):
    with open(f"{SAMPLES}/occ/{name}") as f:
        return f.read()


def _holes(brep):
    return [f for f in te.recognize(brep) if f.type == te.FeatureType.Hole]


@pytest.mark.parametrize(
    "junk",
    ["", "not a brep at all", "CASCADE Topology V1, (c) Matra-Datavision\nLocations 3\n1\n"],
)
def test_junk_is_refused(junk):
    with pytest.raises(RuntimeError):
        te.analyze(junk)


def test_good_part_isnt_repaired():
    assert not _graph(block()).repaired


@needs_samples
def test_broken_file_gets_repaired():
    g = te.analyze(_sample("Top.brep"))
    assert g.repaired
    assert g.face_count == 324


@pytest.mark.parametrize(
    "part,shells,genus",
    [(block, 1, 0), (ball, 1, 0), (filleted_block, 1, 0), (ring, 1, 1),
     (plate_with_holes, 1, 4), (two_blocks, 2, 0)],
)
def test_genus(part, shells, genus):
    g = _graph(part())
    assert (g.shell_count, g.genus) == (shells, genus)
    assert g.euler == 2 * (shells - genus)


def test_ball_poles_arent_edges():
    g = _graph(ball())
    assert (g.face_count, g.edge_count, g.vertex_count) == (1, 1, 2)


def test_glued_blocks_non_manifold():
    g = _graph(glued_blocks())
    assert g.non_manifold_edge_count == 4
    assert g.genus == -1


def test_sheet_has_free_edges():
    g = _graph(flat_sheet())
    assert g.free_edge_count == 4
    assert g.genus == -1


def test_bspline_pocket_rim_convex_floor_concave():
    g = _graph(lofted_pocket())
    z = {f.id: f.center[2] for f in g.faces}
    walls = {f.id for f in g.faces if f.surface_type == "BSpline"}
    assert walls
    assert all(a.kind != te.EdgeKind.Unknown for a in g.arcs)
    rim = [a.kind for a in g.arcs if {a.face_a, a.face_b} & walls and 10 in (z[a.face_a], z[a.face_b])]
    floor = [a.kind for a in g.arcs if {a.face_a, a.face_b} & walls and 2 in (z[a.face_a], z[a.face_b])]
    assert rim and set(rim) == {te.EdgeKind.Convex}
    assert floor and set(floor) == {te.EdgeKind.Concave}


def test_angle_tol_is_tight_on_clean_parts():
    assert all(0 < a.angle_tol_deg < 1e-3 for a in _graph(filleted_bracket()).arcs)


@needs_samples
def test_one_sloppy_edge_doesnt_spoil_the_rest():
    brep = _sample("mal_tige.brep")
    g = te.analyze(brep)
    assert all(a.kind != te.EdgeKind.Unknown for a in g.arcs)
    assert g.genus == 2
    assert len(_holes(brep)) == 2


def test_step_round_trip():
    part = filleted_bracket()
    with tempfile.TemporaryDirectory() as d:
        export_step(part, f"{d}/bracket.step")
        g = te.analyze(step_to_brep_string(f"{d}/bracket.step"))
    want = _graph(part)
    assert (g.face_count, g.edge_count, g.vertex_count, g.genus) == (
        want.face_count, want.edge_count, want.vertex_count, want.genus)


@needs_samples
def test_real_step_file():
    brep = step_to_brep_string(f"{SAMPLES}/step/linkrods.step")
    assert te.analyze(brep).genus == 2
    assert len(_holes(brep)) == 2


@needs_samples
@pytest.mark.parametrize("path", sorted(glob.glob(f"{SAMPLES}/occ/*.brep")), ids=os.path.basename)
def test_every_sample_loads(path):
    with open(path) as f:
        brep = f.read()
    g = te.analyze(brep)
    te.recognize(brep)
    assert all(a.kind != te.EdgeKind.Unknown for a in g.arcs)
    if g.free_edge_count == 0 and g.non_manifold_edge_count == 0 and g.shell_count:
        assert g.genus >= 0
