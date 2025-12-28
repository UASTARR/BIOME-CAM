# Raspberry Pi Camera Activation/Deactivation
This is a project with the purpose of recording video during the descent of the BIOME IV payload. 
The project senses input on GPIO pins 23 and 24 of the Raspberry Pi Zero 2 W.  
On the rising edge of GPIO 23 (recording enable pin), `rpicam-vid` begins running, creating 120000ms (2 minute) segments of recorded flight.  
On the rising edge of GPIO 24 (recording disable pin), the program sleeps for 2 minutes, to guarantee the final segment is written to memory, and `rpicam-vid` is killed. Then, `ffmpeg` is used to convert the generated segments into a single useable video.  
The GPIO inputs will be generated from our STM32 flight computer. At apogee, flight recording will be enabled (as the payload is ejected from the rocket), and upon landing, flight recording will be disabled.  
It may make sense to not run ffmpeg in the program if it's possible we will run out of disk space. It'll be easy to just run ffmpeg on it's own from another computer to convert the generated mkv files into a single file.

### Dependencies
Requires `libcamera`, `libgpiod` (both for raspberry pi), `meson`, and `ninja`.

### Compilation
First, create the build files with `meson build`, then enter the `build` directory; `cd build`, and run `ninja`.
