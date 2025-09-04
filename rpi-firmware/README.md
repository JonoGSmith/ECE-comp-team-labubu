# Firmware
Setting up:

#### Development dependencies
- VSCode
    - Raspberry Pi Pico Extension
    - C/C++
    - CMake Tools
- On Windows, install `msys2`(ucrt) for package management. MacOS, install `brew`. On linux, the native `apt`/`pacman` is fine. Ensure you have the following things installed.
    - CMake
    - A build script backend (ninja/make/visual studio)
- Python3 is installed

I'm fairly sure the Pico Extension will install all of the dependencies for you on windows and macos.

#### Development guide
Once you've built the project once with CMake-Tools use "C/C++: Select Intellisense Configuration" from the command palette and select "CMake Tools". The code should correctly syntax highlight from that point on.

