# Gumball OS

Gumball is a terminal-only operating system. You'll have no distractions, no browser, no apps, no programs. You can run simple commands like file system stuff (`cat`, `touch`, `ls`). The list of capabilities are below.

## Basic Commands
As of writing, the stuff you can do in Gumball are:

Arrow up and down key to see commands history.
| command | usage | description |
| --- | --- | --- |
| `help` | `help` | Show the available commands |
| `clear` | `clear` | Clear the screen |
| `version` | `version` | Show Gumball version |
| `echo` | `echo [TEXT]` | Print text arguments to screen |
| `echo` | `echo [TEXT] > [FILE]` | Write text to a file | 
| `touch` | `touch [FILE]` | Create a new empty file |
| `ls` | `ls` | List files in current directory |
| `rm` | `rm [FILE]` | Remove a file from disk |
| `cat` | `cat [FILE]` | Display file contents to screen | 


## Limitations and Future Plans
Gumball is in early development. There are still missing commands/abilities like changing directory (`cd`), making directories (`mkdir`), file renaming (`mv`), and others.

This project started because I wanted to create my own operating system that's stripped down to the basics. No desktop, no apps, no distractions. I'll eventually add my own programs to this like [`mark`](https://github.com/pagecoy/mark.git) and some others that will make this project more interesting.

Gumball has no storage drivers so it can't read/write actual disks yet. Worked around that below.

## System Choices & Setup
### Gumball File System (GFS1)
Gumball File System 1 or GFS1 is Gumball's file system. In version 1.0.0 of Gumball I'm using a virtual disk that's being loaded to RAM by GRUB. The file is called `disk.img`, that's what Gumball uses as storage. It's not persistent so it's empty after every reboot. If you want to see the code for initializing GSF1 visit `/kernel/fs/fs.c` > `void fs_init(...) {`.

For more file system information go to `GUMBALLSYS.md`. If you can't find it, I didn't make it yet.

### How to Test Gumball
Make sure you have `qemu-system-x86`, `grub-file`,  `grub-pc-bin`, `xorriso`, `mtools`, `gcc`, `build-essentials` installed
```bash
git clone https://github.com/pagecoy/gumball-os.git
cd gumball-os
chmod +x build.sh # or make it a runable program on what os you are on
./build.sh
qemu-system-i386 -cdrom gumball.iso
```
Have fun!