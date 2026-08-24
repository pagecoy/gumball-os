# Gumball OS v1.2.1

Gumball is a terminal-only operating system. You'll have no distractions, no browser, no apps, no programs. You can run simple commands like file system stuff (`cat`, `touch`, `ls`, `mkdir`, `cd`). The list of capabilities are below.

## Basic Commands
As of writing, the stuff you can do in Gumball are:

Arrow up and down key to see commands history.
| command | usage | description |
| --- | --- | --- |
| `help` | `help` | Show the available commands |
| `clear` | `clear` | Clear the screen |
| `version` | `version` | Show Gumball version |
| `echo` | `echo [TEXT]` | Print text arguments to screen |
| `echo` | `echo [TEXT] > [FILE]` | Write text to a file (accepts nested paths, e.g. `home/notes.txt`) |
| `touch` | `touch [FILE]` | Create a new empty file (accepts nested paths) |
| `mkdir` | `mkdir [DIR]` | Create a directory (accepts nested paths) |
| `cd` | `cd [DIR\|..\|/]` | Change directory.|
| `pwd` | `pwd` | Print the current directory path |
| `ls` | `ls [DIR]` | List files. Pass a path to list another directory without moving into it |
| `rm` | `rm [-rf] [FILE]` | Remove a file or directory. `-r` deletes non-empty directories, `-f` ignores missing targets |
| `mv` | `mv [OLD] [NEW]` | Rename a file, or move it into another directory |
| `cat` | `cat [FILE]` | Display file contents to screen (accepts nested paths) |
| `debug` | `debug` | Dump raw filesystem state, for development use |

## Limitations and Future Plans
Gumball is in early development. Still on the todo list: a nano-like text editor, real memory management, a `df`-style disk usage command, custom VGA fonts, and GFS2 fragmentation cleanup, some other commands.

This project started because I wanted to create my own operating system that's stripped down to the basics. No desktop, no apps, no distractions. I'll eventually add my own programs to this like [`mark`](https://github.com/pagecoy/mark.git) and some others that will make this project more interesting.

Gumball has no storage drivers so it can't read/write actual disks yet. Worked around that below.

## System Choices & Setup
### Gumball File System (GFS2)
Gumball File System 2, or GFS2, is Gumball's file system - a rewrite of the original GFS1 that added real directory support with stable file references. Full details, including how files and directories are structured internally, are in [`GFS.md`](./GFS.md).

Gumball currently runs on a virtual disk (`disk.img`) that GRUB loads straight into RAM - there's no real disk driver yet, so nothing persists across a reboot; the disk is freshly formatted every time Gumball boots. If you want to see the code for initializing GFS2, visit `/kernel/fs/fs.c` > `void fs_init(...) {`.

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