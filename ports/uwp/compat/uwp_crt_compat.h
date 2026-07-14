#ifndef DOOM64EX_UWP_CRT_COMPAT_H
#define DOOM64EX_UWP_CRT_COMPAT_H

#include <direct.h>
#include <io.h>
#include <stdarg.h>
#include <stdint.h>

#define access _access
#define close _close
#define getcwd _getcwd
#define open _open
#define read _read
#define write _write

#endif
