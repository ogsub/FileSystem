# FileSystem
 Multithreaded project that simulate file system

## Introduction
 This project is made to simulate simple file system. It requires input file which you can modify using the simulated file system. Some of the file operations that are present includes:
    -mount/unmount partition
    -format partition
    -open file (in various modes - Write, Read, Append)
    -check if file exists
    -write
    -seek
    -check if EOF
    -truncate
and so on.
Program uses two threads that open the same file, read from it and write to new files concurrently.

## Instruction
 Directory tests/ contains test files. Copy one to FileSystem/ directory and rename it to "ulaz.dat". Start the program using Visual Studio. After program completes there should be two output files "izlaz1.dat" and "izlaz2.dat". Rename them to have extension same as input file before renaming to "ulaz.dat". Two threads read different parts of input file and write it to two new files ("izlaz1.dat" and "izlaz2.dat") each with different order, e.g. in some of the output file first the second part of input file is written, then cursor of the output file is moved to the beginning and then the first part of input file is appended, so we should get the same output file as the input one. Almost the same is done for the second output file, but methods used to read input and write to the second output are different. Both output files should be same as input one.

### Info
 Developed using Visual Studio 2019.
