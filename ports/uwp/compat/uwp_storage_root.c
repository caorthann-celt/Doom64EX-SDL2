#include "uwp_storage_root.h"

#include <Windows.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

const char *doom64ex_uwp_content_root(void)
{
    return "E:\\doom64ex";
}

const char *doom64ex_uwp_find_content_file(const char *filename)
{
    static char path[MAX_PATH];
    int length;

    if (!filename || !filename[0] || strchr(filename, ':') || strchr(filename, '\\') || strchr(filename, '/')) {
        return filename;
    }
    length = snprintf(path, sizeof(path), "%s\\%s", doom64ex_uwp_content_root(), filename);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return filename;
    }

    return access(path, 0) == 0 ? path : filename;
}
