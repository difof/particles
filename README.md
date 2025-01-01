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

- [ ] Dependencies
    - imgui
    - raylib
    - rlimgui
    - fmt
    - nlohmann/json
    - tinydir
    - catch2
- [ ] raylib + imgui window helloworld
- [ ] basic unit test for example
- [ ] draw into render texture and into window
- [ ] draw configurable via imgui