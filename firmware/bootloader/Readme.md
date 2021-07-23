
# Installing the firmware

The firmware consists of two parts: the bootloader and the control program.

The bootloader provides the capability to program the (remainder of the) memory via USB. The control program is an Arduino Sketch program that will be started by the bootloader unless halted for (re)programming.

This manual is geared to Windows users using OpenOCD and an ST-Link programmer.

## TL;DR

 1. Get an ST-Link V2 compatible programmer. There are several cheap clones to find online (e.g. [this one by Adafruit](https://www.adafruit.com/product/2548) provides plenty of documentation). \
 Note: whichever programmer you pick: **Make sure it is a SWD programmer**, containing the required SWCLK and SWDIO pins. ( There are JTAG variants out there _without SWD_ support, they _will not work_ for the SAMD chips. )
 2. You might need to tune your Windows installation for the ST-Link to show up correctly. Make sure it is detected and uses the `libusbK` driver. Install this USB driver using the  [Zadig tool](https://zadig.akeo.ie/).
 3. Install [OpenOCD for Windows](http://openocd.org/getting-openocd/). Make sure it is at least version 0.11.0 (contains bugfixes for the at91samdxx chips).
 4. Inspect the provided `openocd.cfg`, and run `openocd` to do a minimal test of the programmer without writing anything.
 5. Hook up the SWCLK, SWDIO, GND, 3V3 pins to the board (RST seems irrelevant).
 6. If confident, comment(-out) lines in `openocd.cfg` to flash the content of `bootloader.bin` into firmware memory. This binary is an [UF2 Bootloader](https://learn.adafruit.com/adafruit-trinket-m0-circuitpython-arduino/uf2-bootloader-details) which simplifies further programming.
 7. Connect via USB and the device should show up as a `Adafruit Trinket M0` device in your PC if successful. The programmer is now no longer needed.
 8. Use the USB connection to upload the control firmware using [Arduino IDE](https://www.arduino.cc/en/guide/windows) and the provided `bias_with_offset.ino` code.
 9. The device should reboot and with the Serial Monitor of the Arduino IDE, you can test programming that should respond to the serial commands `name?` etc.

Done!

Below follow some notes in case of troubleshooting.

## Installing OpenOCD for Windows

There is a number of ways to install OpenOCD and for Windows, the most convient method by far is to [get one of the prebuild binaries available](http://openocd.org/getting-openocd/), rather than compiling it yourself.

If you are an [MSYS2](https://www.msys2.org/) user, you'll probably would like know they have a [mingq-w64-openocd](https://packages.msys2.org/base/mingw-w64-openocd) package as well.

In this how-to, we suggest to download the mingw32 (Windows) build of the [latest release of the offical github mirror](https://github.com/ntfreak/openocd/releases).

 * Make sure to get at least version 0.11.0 to get some bugs fixed in OpenOCD for this at91samdxx chip family.

 * Download the ``.tar.gz`` file, and extract its content (e.g. using [7-zip](https://www.7-zip.org/)) to a convenient location, why not to a subdir of the current directory:
 ```
 ./openocd-v0.11.0-i686-w64-mingw32
 ```

## Testing OpenOCD installation

If OpenOCD is installed in a subdir as above, then you can run the binary from windows command line interface directly (open one using `Shift + Right-click` > `Open command window`).

First, running with option `--version` should show something like this:
```
C:\...\sipm-bias-control\firmware\bootloader>"openocd-v0.11.0-i686-w64-mingw32\bin\openocd.exe" --version
Open On-Chip Debugger 0.11.0 (2021-03-07-12:52)
Licensed under GNU GPL v2
For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
```

 > Note: You can also extract the OpenOCD installation directory anywhere else and add that path ending in `...\openocd-v0.11.0-i686-w64-mingw32\bin` to your Windows' PATH Environment variable, then you'd only need to type `openocd` (might need a restart of your command line).
 
## Running OpenOCD

Notice the ```openocd.cfg``` file present in the current directory, open it to see the content:

```
source [find interface/stlink.cfg]
transport select hla_swd
set CHIPNAME at91samd21e18
set CPUTAPID 0x0bc11477
source [find target/at91samdXX.cfg]
# # MINIMAL TEST (comment out next three lines when ready)
init
targets
exit
# # PROGRAM AND VERIFY (uncomment the rest when ready)
#init
#reset halt
#at91samd chip-erase
#at91samd bootloader 0
#load_image {bootloader.bin} 0x00000000 bin
#verify_image {bootloader.bin} 0x00000000 bin
#at91samd bootloader 8192
#reset run
#exit
```

If `openocd` is run and there exists a `openocd.cfg` file (it is a script written in TCL). `openocd` will execute each line in this file and you can comment out lines by inserting `#`. You may in fact provide all the commands via command line interface.

As you can read, it is recommended to first test the commands listed under the comment `# MINIMAL TEST` and see if this works error-free before attempting to program.

You may have noticed that the commands starting with `source` refer to other `.cfg` files, you may find these below your OpenOCD installation prefix, under `share\openocd\scripts`.

The command `help` will list all the commands understood for this target with small desciptions.
