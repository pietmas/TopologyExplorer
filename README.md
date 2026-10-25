# Topology Explorer

I'm learning how CAD kernels work by writing a small one.

C++ on top of OpenCASCADE, called from Python.

## Why

I know a bit of CAD and I got interested in what's behind it. Every
modeller sits on a kernel that holds the shape and answers questions
about it. That's the part I want to see.

I want to get my hands dirty and explore it properly, so I'm building
a small one and checking it against shapes where I already know the
answer.

## How

    python                      C++
    ------                      ---
    make a part
       |
       |  brep text
       +-------------------->   read it
                                count faces, edges, vertices
                                see which faces touch
                                convex or concave?
                                hole, fillet, pocket, boss?
                                remove them if asked
       |
       +<-------------------    numbers back (and brep text)
    check them, look at them

Only text crosses. No objects.

## The main idea

Faces are nodes, shared edges are arcs. Each arc is convex, concave or
smooth.

I first thought a hole was "a cylinder where every edge is concave".
It isn't. The rim of a drilled hole is a sharp outside corner, so it's
convex. The hole's surface curves around empty space, its edges don't.
A hole is a cylinder you're standing inside of: its outward normal
points at its own axis, and it goes all the way round.

Geometry says where things are. Topology says how they connect.

## What it does

    te.analyze(brep)               face graph: faces, arcs, counts, euler, genus
    te.recognize(brep)             holes, fillets, pockets, bosses
    te.defeature(brep, face_ids)   remove faces, say what happened to the rest

Face ids are just positions in a list. Defeature anything and they
shuffle. The face map is the only thing that links old to new.

Broken shapes go through ShapeFix first, `repaired` says if it had
to. If that doesn't fix them they're refused.

Genus is from `V - E + F - L = 2(S - G)`, L inner loops, S shells.
-1 on open sheets and non-manifold stuff, where it means nothing.

STEP files: `bridge.step_to_brep_string(path)`.

`defeature` gives back `(ok, brep, face_map)`. Every old face is
Kept, Modified, Deleted or Lost, with its new ids if it survived.
If OpenCASCADE can't do it, `ok` is False and the brep is empty.

## Build

Needs OpenCASCADE dev packages (`libocct-*-dev` on Ubuntu).

    python3 -m venv .venv
    CMAKE_ARGS="-DOpenCASCADE_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencascade" \
      .venv/bin/pip install -e ".[dev]"

    cd python && ../.venv/bin/python -m pytest -q

Rebuild after any C++ change. Some tests use the sample parts from
`occt-misc` and skip without it.

## Look at it

    .venv/bin/python -m ocp_viewer --port 3939     # then open http://127.0.0.1:3939/viewer

    cd python
    ../.venv/bin/python viz.py filleted_bracket features
    ../.venv/bin/python viz.py plate_with_holes defeature
    ../.venv/bin/python viz.py filleted_block surface

Any function in `parts.py` works as the part. Faces are labelled with
their C++ id in the viewer's tree.

## Not yet

- defeature gives up on a hole split in two halves
- no chamfers, counterbores or slots
- pockets only with walls at 90 degrees
