# Pictomage

A top-down 2D puzzle-shooter game. Made in 1 week for the [Raylib 5K gamejam](https://itch.io/jam/raylib-5k-gamejam). Play it from your browser on [itch.io](https://blatnik.itch.io/pictomage), or download a native binary from the [Releases](https://github.com/blat-blatnik/Pictomage/releases) page.

<p align="center">
  <img src="./screenshots/5.png" alt="Screenshot" width="500"/>
</p>

## System requirements

- WASM capable web browser (for the browser version).
- 1 GHz CPU.
- 256 MB of RAM.
- OpenGL 3.3 / DirectX 10 capable GPU.

## How to build (Web)

This all assumes you are using Windows 10. You will need the Emscripten SDK:

1. Go to the [emsdk github page](https://github.com/emscripten-core/emsdk).
2. Click on the green `Code` dropdown and then `Download ZIP`.
3. Extract the zip to a folder called `Emscripten`, and put that folder wherever you want.
4. In the start menu search bar, search for `path`. Then click on `Edit the system environment variables`.
5. In the `System variables` list, find the `Path` variable, select it, then click `Edit`.
6. Press `New`, and type out the path to the `Emscripten` folder (e.g. `C:\dev\Emscripten`).
7. Press `Ok` to close all the open windows.
8. To make sure everything worked, open up a new command prompt, and run `emsdk`. It should say `Missing command; Type 'emsdk help'..`
8. In the same command prompt, run `emsdk update` followed by `emsdk install latest`. 
9. You can now just run [`webbuild.bat`](./webbuild.bat) inside this project to build the game. The output goes into [`bin/web/`](./bin/web/).
10. Run [`webrun.bat`](./webrun.bat) to run the game in your browser.

## How to build (Windows - Visual Studio)

1. You need to download and install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/).
2. When you need to select which components to install, make sure `Desktop development with C++` is ticked.
3. Open up [`Pictomage.sln`](./Pictomage.sln) in Visual Studio and click the run button at the top. The output goes into [`bin/windows/`](./bin/windows/)

## How to build (Mac)

Open a terminal at the root of the project, and compile the code with clang:

```bash
$ clang -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL src/*.c -L./lib -lraylib_mac_x64 -o Pictomage -target x86_64-apple-macos10.12
```

This will produce an x64 executable, `Pictomage`, that you can run. If you want to compile for arm64 (Apple M1), run the following instead:

```bash
$ clang -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL src/*.c -L./lib -lraylib_mac_arm64 -o Pictomage -target arm64-apple-macos11
```

The [`macbuild.sh`](./macbuild.sh) script will compile the code for both targets, package it into a universal binary, make an app bundle, as well as a DMG installer. Run it like this:

```bash
$ chmod +x macbuild.sh
$ ./macbuild.sh
```

The output will be in [`bin/mac/`](./bin/mac).

## Credits

The coding and level design was done by Blat Blatnik. The art was made by Olga Ważny. Many thanks to all the friends that playtested the game!

### Sound effects

| name          | filename             | original author                            | source |
|---------------|----------------------|--------------------------------------------|--------|
| Explosion     | `explosion.wav`      | EFlexMusic (boyermanproductionz@gmail.com) | [freesound.org](https://freesound.org/people/EFlexMusic/sounds/388528/)
| Glass shatter | `shatter.wav`        | Profispiesser                              | [freesound.org](https://freesound.org/people/Profispiesser/sounds/583065/)
| Camera flash  | `snap2.wav`          | HughGuiney (@LordPancreas)                 | [freesound.org](https://freesound.org/people/HughGuiney/sounds/352832/)
| Release       | `eject.wav`          | Iridiuss                                   | [freesound.org](https://freesound.org/people/Iridiuss/sounds/519414/)
| Turret hit    | `turret-destroy.wav` | TROLlox_78                                 | [freesound.org](https://freesound.org/people/TROLlox_78/sounds/274119/)
| Turret shot   | `long-shot.wav`      | morganpurkis                               | [freesound.org](https://freesound.org/people/morganpurkis/sounds/388194/)
| Bullet hit    | `bullet-wall.wav`    | FilmmakersManual                           | [freesound.org](https://freesound.org/people/FilmmakersManual/sounds/522402/)
| Ear ringing   | `ringing1.wav`       | ArrowheadProductions                       | [freesound.org](https://freesound.org/people/ArrowheadProductions/sounds/547974/)
| Teleport 1    | `teleport.wav`       | steaq                                      | [freesound.org](https://freesound.org/people/steaq/sounds/560124/)
| Teleport 2    | `teleport.wav`       | krzysiunet                                 | [freesound.org](https://freesound.org/people/krzysiunet/sounds/345835/)
| Teleport 3    | `teleport.wav`       | DWOBoyle                                   | [freesound.org](https://freesound.org/people/DWOBoyle/sounds/474180/)

### Libraries

The game was made with [Raylib](https://www.raylib.com/). Raylib was made by Ray (@raysan5) and many open source contributors.