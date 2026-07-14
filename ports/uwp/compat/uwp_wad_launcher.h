#ifndef DOOM64EX_UWP_WAD_LAUNCHER_H
#define DOOM64EX_UWP_WAD_LAUNCHER_H

#include <string>
#include <vector>

bool doom64ex_uwp_run_wad_launcher(const wchar_t *local_state, std::vector<std::wstring> &selected_wads);
std::string doom64ex_uwp_narrow_path(const std::wstring &path);

#endif
