# Particle Simulator

Some particle simulation, based on some dude's basic idea on internet.
Got a little obsessed and added a bunch of random stuff.
Since the stupid little shit of Apple doesn't support OpenGL compute shaders, I will never parallelize using GPGPU.
Why? Because Apple is a little silly fucking shit sometimes, and their Metal API + ObjectiveC feels like coding in r*st.
And Vulkan? That will probably take me a month to just draw a rectangle in and I'm super lazy for some crap like that for such small project.
So, here we are, stuff being simulated on CPU cores - 2. The good thing is I can later on add headless rendering, to like .. render big worlds on a server.

I haven't stress tested, but my simple M1 can run 15k particles at ~80 TPS, though N=15k is way too much and chaotic.
Bigger group radius and more cramped particles eat the TPS, because of obvious reasons, like more interactions should be mathed each step.
Uniform grids are a big deal in speeding the sim up. Actually that's the only real optimization I added, apart from parallelism.
So bigger cells need more compute, and more particles in each cell need even more compute.

The idea is simple, for each particle in the world, apply neighbour particles's forces in a radius on self.
And this creates some crazy emergent behavior with just a few force rules! Kinda works like real world particles like electrons and shit, I guess.
Later on I will try making the simulation less static and boring, because once the "creatures" form, nothing new would happen. 
They would just move around, consume or create other "creatures" and the cycle repeats. By "creature" I mean cell like life like beings
that emerge with a bunch of particles. I won't share screen shots because you must compile and see for yourself.
I won't even point out the website of such simulation which you can render in your browser. Because you must put the effort in finding things out.

# Build

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
task run-release
```

# TODO

- move seed into something else and keep a base in simulation to reset to, base will be updated if theres new seed. new commands: update existing rules (no reset), upload seed (reset)
- grid inside world
    - world interface for external access (out of sim class)
- break down main
    - render class
    - ui class
        - break down ui
        - windows for ui parts
- clean up world class as it's the very first thing ever made in this shit
- save load rules and settings to/from json
- have a whole look at the var naming etc, make them clear snake_case
- density heatmap is ok, but also add temp map (cells with a lot of movement)
- 2d camera and movable texture
- resizable texture & bounds
- screenshot & video
- use lua programmable seeds
- probably use luajit for kernels!
- more emergent behavior

# FIXME

- DrawRegionInspector and render_tex share same interpolation logic. repeating myself
- draw grids and velocities in a different layer/render target
- target tps is not accurately capping the exact tps
- when reset, total steps wont zero

# License
Don't fork, don't copy, don't sell. I'll find you and feed your entire bloodline to the chicken.