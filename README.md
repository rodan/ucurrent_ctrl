
## µCurrent controller

a MSP430F5510 based project that controls the power delivery to Dave L. Jones's µCurrent.

![Lib Logo](./doc/img/current_ctrl_logo.png)

```
 homepage:        https://github.com/rodan/ucurrent_ctrl
 author:          Petre Rodan <2b4eda@subdimension.ro>
 license:         GNU GPLv3
```

if you forget to turn off the original µCurrent you will find the enclosed 2032 Li cell completely flat the next day.
this project provides a timeout feature to it. a push-button is used to start up both this circuit and the µCurrent.
multiple presses will either reset the timeout counter or increase the timeout period.
the initial timeout period can be optionally modified via a 3V serial uart.
also in case the output voltage is low enough for a given period the µCurrent is turned off.


### uC pinout:

* P1.1  IN  digital - push button trigger
* P1.4  OUT digital - power-latch my circuit
* P1.5  OUT digital - enable power delivery to the uCurrent
* P2.0  OUT digital - blinky
* P6.0  IN  analog  - battery voltage
* P4.1  IO          - SDA (i2c for ADS1110)
* P4.2  IO          - SCL


### connectors:

* external power - either 3V Li Cell or 3.7V LiPo/Li-ion (MOLEX 0530470210)
* push button    - optional external push button tied to the case (MOLEX 0530470210)
* ucurrent power - uCurrent connections (MOLEX 0530470210)
* debug          - TI spy-bi-wire interface and serial uart for programming, debug and user input (MOLEX 0530470610)


### power:

* the uCurrent has the cell unpopulated and the power switch set to the "ON" position.
* this circuit will provide the 3V needed to the uCurent


### software requirements:

* [TI msp430 toolchain](https://www.ti.com/tool/MSP430-GCC-OPENSOURCE)
* [atlas430](https://github.com/rodan/atlas430) HAL library


### build and debug

the project is based on a [Makefile](./firmware/Makefile). **REFLIB_ROOT** defines the path to where this atlas430 repository has been cloned, **TARGET** represents the target microcontroller and [config.h](./firmware/config.h) will be automatically expanded into compilation macros (-DFOO arguments to be sent to gcc).

the makefile supports the following options:

```
# check if everything is installed and paths are correct
make envcheck

# remove the build/TARGET/ directory
make clean

# compile the project and library
# create dependency tree and source code tags
make

# burn the firmware onto the target microcontroller
make install

# perform an optional static scan of the source code 
make cppcheck    # needs dev-util/cppcheck
make scan-build  # needs sys-devel/clang +static-analyzer
```

in order to use gdb to debug the project make sure to enable the **CONFIG_DEBUG** macro in config.h and run in a terminal

```
LD_PRELOAD='/opt/atlas430/lib/libmsp430.so' mspdebug --allow-fw-update tilib gdb
```

and then start gdb from within the project directory:

```
make debug
```

commonly used commands from within gdb provided as example for the unit tests:

```
target remote localhost:2000
monitor reset
monitor erase
load build/MSP430FR5994/main.elf
b
disassemble
nexti
info registers
continue
tui enable
```

