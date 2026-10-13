# Huenicorn

A free Philips Hue™ screen synchronizer for Gnu/Linux

<p align="center">
  <img src="assets/logo.svg" alt="Logo" width="256" height="256">
</p>

## Description

Huenicorn is a free ambilight driver for Gnu/Linux.
In other words, Huenicorn allows you to extend the colorful atmosphere of your computer screen to your Hue light bulbs in real-time.
Huenicorn provides a simple web interface to assign specific portions of screen to the light bulbs you desire and save the settings for further entertainment sessions.

### Screenshot

| ![Screenshot](screenshots/HuenicornFullWebUI.png) |
|:--:|
| *Caption: Huenicorn Light Manager user interface* |

## Project status

Huenicorn 1.2.0 is available.

### This new version brings

- New features
  - Gamescope support
  - Transition smoothing (with control in UI's advanced settings)

- Fixes and improvements
  - Support for MbedTLS 4.x
  - Fix restoreToken update
  - Fix config serialization
  - Fix busy loop in xdg-portal

## Getting Started

 Before using Huenicorn, you need to define some entertainment area on your Hue bridge through the official application provided by Philips.

### Requirements

* Gnu/Linux system running with graphics sessions based on X11 or Wayland
* Philips Hue bridge with registered lamps

### Dependencies

* [X.Org](https://xorg.freedesktop.org) or [Wayland](https://wayland.freedesktop.org)
* [OpenCV](https://github.com/opencv/opencv)
* [Mbed-TLS](https://github.com/Mbed-TLS/mbedtls)
* [Curl](https://curl.se)

#### Dependencies intallation

<details>

<summary>ArchLinux</summary>

```bash
# Required dependencies
sudo pacman -S git cmake make gcc curl opencv mbedtls

# For X11 support
sudo pacman -S xorg-server

# For Wayland support
sudo pacman -S wayland glib2 pipewire
```

</details>

<details>

<summary>Fedora</summary>

```bash
# Required dependencies
sudo dnf install -y git cmake gcc gcc-c++ opencv-devel curl-devel mbedtls-devel

# For X11 support
sudo dnf install -y libXrandr-devel

# For Wayland support
sudo dnf install -y pipewire-devel glib2-devel
# Extract the archive and copy its content to the target directories:
```

</details>

<details>

<summary>OpenSUSE Tumbleweed</summary>

These dependencies needed to be installed on OpenSUSE Tumbleweed 20231011 to build and run Huenicorn:

```bash
sudo zypper install opencv-devel libopencv408 python311-jsonschema

# For Wayland support
sudo zypper install pipewire-devel glib2-devel

```

Additionally you have to build Mbed-TLS from source from the links above.
Follow the build instructions in their respective README files and copy them to the appropriate place, as some of them don't do that automatically (usually /usr/local/lib64/ for libraries (check LD_LIBRARY_PATH) or /usr/local/include/ for includes)

</details>

<details>

<summary>Ubuntu 26.04 LTS and newer</summary>

```bash
# Required dependencies
sudo apt-get install build-essential cmake libopencv-dev libcurl4-openssl-dev libmbedtls-dev

# For X11 support
sudo apt-get install libx11-dev libxext-dev libxrandr-dev

# For Wayland support
sudo apt-get install libglib2.0-dev libpipewire-0.3-dev wayland-utils
```

</details>

<details>
<summary>Ubuntu : 22.04 - 24.04</summary>

```bash
# Some Ubuntu installations may require the Universe repository
sudo add-apt-repository universe
sudo apt-get update

# Required dependencies
sudo apt-get install build-essential cmake libopencv-dev libcurl4-openssl-dev libmbedtls-dev

# For X11 support
sudo apt-get install libx11-dev libxext-dev libxrandr-dev

# For Wayland support
sudo apt-get install libglib2.0-dev libpipewire-0.3-dev wayland-utils

# Huenicorn requires GCC/G++ 12 or newer.
# Ubuntu 22.04 ships GCC 11 by default, so install GCC 12 or later if needed.
# If your current version of GCC/G++ is < 12, you need to install it
sudo apt install gcc-12 g++-12
# And then this command will be required instead of ```cmake ..``` in the "Building Huenicorn" chapter
cmake -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 ..
```

Versions of Ubuntu earlier than 24.04 are not officially supported. Please refer to [This post](https://gitlab.com/openjowelsofts/huenicorn/-/issues/5#note_1700387996) if you still want to give it a try.
</details>

<details>
<summary>NixOS</summary>

A Nix Flake is provided with this repository:

```bash
nix run gitlab:openjowelsofts/huenicorn
```
</details>

### Building Huenicorn

```bash
git clone https://gitlab.com/openjowelsofts/huenicorn.git
cd huenicorn
mkdir build && cd build
# Use this line for default configuration
cmake ..
# or this one to disable automatic online-fetching fallback for dependencies (glm, nlohmann_json, httplib)
cmake -DHUENICORN_FETCH_DEPS=OFF ..
make
```

The output is a standalone executable called "huenicorn".

### Executing program

* Run the executable named "huenicorn" in the way you prefer (Terminal if you want some text feedback)
* Open your favorite web browser at [127.0.0.1:8215](http://127.0.0.1:8215)
Browser will spawn automatically for the initial setup and as long as no light profile has been saved
(Service port can be edited in configuration. Default is 8215)

#### Initial setup

When executed for the first time, Huenicorn spawns the setup page in your favorite browser. It consists of a step-by-step wizard to locate your Hue bridge on the network and register an user on it.

Upon setup proper setup termination, Huenicorn will resume in its ordinary execution workflow.

The webpage can then be refreshed to reach the Light Manager interface.

#### Ordinary execution worflow

<details>
  <summary>Wayland-specific step</summary>
  <br/>

  At initialization, the grabber will ask you to select some source through a screen sharing portal. Pick the desired screen and validate by clicking "Share".
</details>
<br/>

Huenicorn will automatically select a the suitable screen-grabber for the graphic session.

At startup, Huenicorn will look for a saved profile and start its color streaming execution.

If you want to assign lights to some portions of your screen, open the web interface and drag'n'drop the available lights from the left list to the right one.

When a synced light is selected in the list, its portion of allocated screen can be adjusted in the screen partitioning section.

Once you are satisfied with the layout, you can save it as a profile and it will be automatically reloaded for further executions.

As long as no profile has been saved, light management interface will spawn automatically at startup.

### Configuration files

The configuration files can be found in your home directory at ~/.config/huenicorn/
config.json contains the bridge-related configuration.
profile.json contains the saved channels geometry for an entertainment configuration.

The data structure of these files is JSON.

#### config.json

  This file contains some parameters that can be manually edited after initial setup.

* **boundBackendIP**:  (String) IP address to bind the service on. Defaults to `127.0.0.1` (local access only). Use `0.0.0.0` to reach the web UI from other devices, keeping in mind it has no authentication
* **bridgeAddress**:  (String) Address of the Philips Hue bridge
* **credentials**:
  * **clientkey**: (String) 32 hexadecimal characters key provided by initial setup
  * **username**: (String) Base64 or UUID username provided by initial setup
* **interpolation**:  (Unsigned) Identifier of the desired subsample interpolation function
  * Nearest = 0 : Cheapest. Low quality on high detail. Can cause the lights to flash
  * Cubic = 1 : Slow and harsh (kept for test)
  * Area = 2 : (Recommended) Smooth and respectful of overall content
* **profile**:  (String) Name of the current user-defined light profile
* **refreshRate**:  (Unsigned) Screen capture and light update frequency
* **restoreToken**:  (String) Token to remember last session settings (Wayland only)
* **restServerPort**:  (Unsigned) Port on which the web UI must respond
* **subsampleWidth**:  (Unsigned) Width of the treated image subsample

Here is an example

```json
{
  "boundBackendIP": "127.0.0.1",
  "bridgeAddress": "192.168.0.10",
  "credentials": {
    "clientkey": "01234567890ABCDEF0123456789ABCDE",
    "username": "AbCdEfGhIjKlMnOpQrStUvWxYz-0123456789012"
  },
  "interpolation": 2,
  "profileName": "profile",
  "refreshRate": 60,
  "restoreToken": "aBcDeFgH_iJkLmNoPqRsT1",
  "restServerPort": 8215,
  "subsampleWidth": 32
}
```

#### profile.json

This file contains a list of channels related to an entertainment configuration.

* **Channels**: (Array) List of entertainment configuration channels
  * **active**: (Boolean) whether the channel is active or not
  * **channelId**: (Unsigned) Index of the channel in the entertainment configuration
  * **devices**: (Object) List of devices for the channel
    * **id**: (String) UUID of the device
    * **name**: (String) User-defined name of the device
    * **id**: (String) Manufacturer-defined name of the device
  * **gammaFactor**: (Float) Light brightness modifier
  * **uvs**: (Object) Pair of coorinates for sub image treatment
    * **uvA**: (Object) Top-left corner coordinate
      * **x**: (Float) x coordinate
      * **y**: (Float) y coordinate
    * **uvB**: (Object) Bottom-right corner coordinate
      * **x**: (Float) x coordinate
      * **y**: (Float) y coordinate
* **entertainmentConfigurationId**: (String) UUID of the related entertainment configuration

#### Shutting down

Huenicorn can be shut down through the web interface or by sending a termination signal.

## Troubleshooting

### I can't see/select any entertainment area from the web interface

This can happens if the credentials are invalidated for various reasons and can usually be fixed following these steps:

* Shutdown Huenicorn if running
* Remove (or rename) the ```~/.config/huenicorn/config.json``` file
* Restart Huenicorn
* Redo the initial setup and at the credentials step, press the "I don't have credentials yet" button
* Follow the instructions involving pressing the bridge's physical button and finish the setup

Huenicorn should now be able to properly list Entertainment Areas and stream colors

### My lights display raibow shift instead of my screen color

#### Reason

Huenicorn could not load any Grabber for the desktop session. It then loads the "Dummy grabber" to still allow access to the management panel and ensure bridge communication.

#### Solution

Check missing dependencies for the running session type

If you are unsure of the current session type, enter the following command:

```bash
echo $XDG_SESSION_TYPE
```

Then refer to the dependencies installation section for your distro.

The CMake output tells which Grabber is available for build:

```cmake
cmake ..
...
[cmake] Able to build X11 Grabber !
[cmake] Able to build Pipewire Grabber !
...
```

Wayland requires the ```Pipewire``` Grabber.

### The monitor-selection portal on Wayland doesn't show anymore and I need to change my settings

1. Shutdown Huenicorn if running
2. Open the `~/.config/huenicorn/config.json` file and remove the line regarding `restoreToken`
3. Start Huenicorn
4. The monitor selection portal will spawn again


### Crash after screen selection on Wayland session

There is a known bug affecting Huenicorn depending on build parameters. This problem was "half-solved" since 1.0.9 but can somehow persist under certain circumstances.

Huenicorn can then be rebuilt with different optimisation flags to hopefully end with a functional result.

Clear the content of the ```build``` directory (if it exists).

Reconfigure the project with the following line instead of ~~```cmake ..```~~


```bash
cmake -DCMAKE_BUILD_TYPE=<CMakeBuildType> -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make
```

```<CMakeBuildType>``` has to be replaced with one of the values below:

* Release
* MinSizeRel
* Debug
* RelWithDebInfo

Then try running Huenicorn again to see if it can now go beyond screen selection.

### Huenicorn still crashes immediately

Check the ```~/.config/huenicorn/config.json``` file and check the following properties:

```subsampleWidth``` : Must be higher than 0 and a divisor of your screen resolution.
```refreshRate``` : Must be between 1 and your screen refresh rate

## Website

Additionnal information and news can be found on [Huenicorn.org](http://huenicorn.org), the official website of the project.

## Version history

* 1.2.0 (latest)
  * Add Gamescope support, Add transition smoothing, Add support for MbedTLS 4.0
* 1.1.0
  * Better color brightness/saturation, Bridge Pro support, automatic wayland screen selection
* 1.0.12
  * Global refactor for less dependencies and clearer workflow
* 1.0.11
  * Embed webroot into binary, handle multiple bridges at setup
* 1.0.10
  * Change default port, fix color computation, fix some minor bugs
* 1.0.9
  * Small step forward for fixing wayland crash
* 1.0.8
  * Global code restructuration and logo integration
* 1.0.7
  * Bug fixes for Wayland sessions
* 1.0.6
  * Library replacements and better compiler support
* 1.0.5
  * Add choice between subsampling interpolations in Web UI's advanced settings
* 1.0.4
  * Add support for Wayland graphic sessions
* 1.0.3
  * Move to Mbed-TLS 3.4
* 1.0.2
  * Performance improvement
* 1.0.1
  * Better reliability and documentation
* 1.0.0
  * First stable release
* 0.0.0 (Legacy)
  * Initial (slow) implementation using Hue HTTP API

## Authors

OpenJowel

## License

Huenicorn is licensed under the GNU GPLv3 License. See the [LICENSE.txt](LICENSE.txt) file for details.

## Acknowledgments

* [XDG portal screencast](https://codeberg.org/metamuffin/xdg-dp-start/src/branch/master/xdg-dp-start.c)
* [Pipewire documentation](https://docs.pipewire.org/page_tutorial5.html)
* [RGB D65 conversion](https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d)
* [Huestacean](https://github.com/BradyBrenot/huestacean)
