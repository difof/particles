# Particle Simulator

A real-time particle physics simulation that explores emergent behaviors through simple force based interactions. This project implements a multi-threaded CPU based particle system with spatial optimization techniques.

## Architecture & Design Decisions

**CPU-First Approach**: While GPU compute shaders would offer significant performance benefits, this implementation focuses on CPU parallelism for broader compatibility and easier deployment. The current architecture allows for future headless server side rendering capabilities, enabling large scale simulations without requiring specialized graphics hardware.

**Performance Characteristics**: On Apple M1 hardware, the simulation achieves ~80 TPS (ticks per second) with 15,000 particles. Performance scales inversely with particle density and interaction radius, larger groups and tighter particle packing increase computational complexity due to the quadratic nature of neighbor force calculations.

**Spatial Optimization**: The simulation uses uniform grid spatial partitioning to reduce neighbor search complexity from O(n²) to approximately O(n). This optimization is crucial for maintaining interactive frame rates with larger particle counts.

## Simulation Mechanics

The core simulation applies force based interactions between particles within a defined radius. Each particle calculates forces from neighboring particles, creating emergent behaviors that resemble natural particle systems. Simple force rules generate complex, self organizing patterns that can form stable structures resembling cellular life forms.

The emergent behaviors include particle clustering, dynamic group formation, and self sustaining patterns that evolve over time. Future development aims to introduce more dynamic interactions to prevent static equilibrium states and create more engaging evolutionary behaviors.

# Build (MacOS/Linux)

First install these dependencies:

-   [git](https://google.com/search?q=install+git)
-   [cmake](https://google.com/search?q=install+cmake)
-   [make](https://google.com/search?q=install+make)
-   [gcc or clang](https://google.com/search?q=install+gcc+or+clang)
-   [premake](https://google.com/search?q=install+premake)
-   [taskfile](https://google.com/search?q=install+taskfile)

Then run these:

```sh
git clone https://github.com/difof/particles
cd particles
git submodule update --init --recursive
task run
```

# TODO

- screenshot & video
- use lua programmable seeds
- probably use luajit for kernels!
- more emergent behavior
- remote view
- integrate metal
- integrate glsl compute

# License
AGLP-3-only