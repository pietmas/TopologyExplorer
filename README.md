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
    make a box
       |
       |  brep text
       +-------------------->   read it
                                count faces, edges, vertices
                                see which faces touch
                                convex or concave?
                                hole or fillet?
       |
       +<-------------------    numbers back
    check them

Only text crosses. No objects.

## The main idea

Faces are nodes, shared edges are arcs. A cylinder where every edge
is concave and it wraps all the way round is a hole.

Geometry says where things are. Topology says how they connect.

## Build

    CMAKE_ARGS="-DOpenCASCADE_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencascade" \
      .venv/bin/pip install -e .

    cd python && ../.venv/bin/python -m pytest -q

Rebuild after any C++ change.
