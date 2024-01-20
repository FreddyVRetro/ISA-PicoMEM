# CuteMouse

CuteMouse is a small open source DOS mouse driver, developed from 1997 to 2009 by Nagy Daniel, Eric Auer, and many others. The project is hosted on [SourceForge](https://cutemouse.sourceforge.net) and released under the terms of the GNU General Public License (GPL).

This repository hosts a version of CuteMouse v2.1b4 converted to compile with Jwasm 2.12 on a modern Linux or Mac OS host. Beside requiring a DOS environment to be compiled, the original version was incompatible with 8088/8086 CPUs since it used a few 286 instructions. I have fixed this incompatibility to make the driver compatible with every x86 CPU.

## License

* CuteMouse ant this repository as a whole are distributed under the terms of the GPL version 2. See the `copying` file for more details. 
* `bin2exe.c` is a modified version of the `bin2exe` utility included in [booterify](https://github.com/raphnet/booterify), developed by Raphaël Assenat and distributed under the MIT License.

