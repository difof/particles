# Particle Simulator


# Build

First install these dependencies:

-   [git](https://google.com/search?q=install+git)
-   [cmake](https://google.com/search?q=install+cmake)
-   [make](https://google.com/search?q=install+make)
-   [gcc](https://google.com/search?q=install+gcc)
-   [premake](https://google.com/search?q=install+premake)

Then run these commands:

```sh
git clone https://github.com/difof/particles
cd particles
git submodule update --init --recursive
premake5 gmake2
make -C build -j8 particles

# run the code
build/bin/Debug/particles
```

# TODO

- [x] Dependencies
    - imgui
    - raylib
    - rlimgui
    - fmt
    - nlohmann/json
    - tinydir
    - catch2
- [x] raylib + imgui window helloworld
- [x] draw into render texture and into window
- [x] draw random circles
- [x] move those circles over time
- [ ] apply the basic formula
- [ ] apply radii cap
- [ ] bounds repel
- [ ] bounds borders
- [ ] mouse drag to move camera
- [ ] mouse scroll to zoom also with + and - keys
- [ ] camera smoothing
- [ ] actions at mouse:
    - B: bomb with repel in area
    - S: spray random stuff
- [ ] optimize sqrt and math
- [ ] smooth the math
- [ ] uniform grid (linked list maybe)
- [ ] vertex shader to make it nice
- [ ] move on to compute shader
- [ ] UI: general stats
    - [ ] fps, timestep, num particles
    - [ ] per particle group stats: num, color
- [ ] UI: editor
    - [ ] world settings
    - [ ] particle editor
    - [ ] save/load config
- [ ] draw lines between particles, width based on bi-directional force
- [ ] heatmap