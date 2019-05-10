# nanoBench

This is an **experimental** fork of the nanoBench Linux x86 microbenchmarking tool. See [the original repo](https://github.com/andreas-abel/nanoBench) for full details; this tool was originally developed by Andreas Abel. I have added support for exception handling (so illegal instructions can be benchmarked) and execution of arbitrary machine code (input as a C string rather than as a binary file as in the original). However, note that this breaks nanoBench's original command-line interface; please use the original repo if you wish to use this interface. The kernel module is also unsupported due to the challenges of exception handling in the kernel.

## Installation

```
sudo apt install msr-tools
git clone https://github.com/cattius/nanoBench.git
cd nanoBench
make
```
*nanoBench might not work if Secure Boot is enabled. [Click here](https://askubuntu.com/a/762255/925982) for instructions on how to disable Secure Boot.*

## Usage

The provided example program is usingNanobench.c, which tests a single illegal instruction (0x0f0b, ud2). Update the configuration options with the length of your instruction in bytes (for exception handling), the instruction itself as a string of hex bytes, and the filepath of your chosen configuration file (this is dependent on the microarchitecture of your CPU; opening the performance counters will fail if you choose a file for a different microarchitecture). If the provided length is incorrect, exception handling will fail and the program will hang.

```
instrLen = 2;
char* instr = "\x0f\x0b";
char* config = "configs/cfg_Broadwell_common.txt";
```

The profileInstr() function tests a single instruction at a time, but can be called repeatedly with different instructions to test (instr and config are passed as parameters). Further configuration options (such as the number of times to test the instruction per measurement, and the number of measurements to make - 10000 and 10 by default respectively) can be found in nanoBench.h.

To compile and run the program, use:

```
make
sudo ./run.sh
```

Counter measurements will be output to the console along with where relevant the signal number (if the instruction causes an exception). Note that if an instruction causes an exception measurements include the overhead of OS-level and user-level exception handling.


## Performance Counter Config Files

Performance counter configuration files for most recent Intel and AMD CPUs are in the `configs` folder. These files can be adapted/reduced to the events you are interested in.

The format of the entries in the configuration files is

    EvtSel.UMASK(.CMSK=...)(.AnyT)(.EDG)(.INV)(.CTR=...)(.MSR_3F6H=...)(.MSR_PF=...)(.MSR_RSP0=...)(.MSR_RSP1=...) Name

You can find details on the meanings of the different parts of the entries in chapters 18 and 19 of [Intel's System Programming Guide](https://software.intel.com/sites/default/files/managed/a4/60/325384-sdm-vol-3abcd.pdf).

## Supported Platforms

This fork supports Intel processors with architectural performance monitoring version ≥ 3. Support has also been implemented for AMD Family 17h processors, but note this has not yet been tested. The code was developed and tested using Ubuntu 18.04.
