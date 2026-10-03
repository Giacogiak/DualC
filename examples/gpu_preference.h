#pragma once
// Opt this process into the discrete GPU on hybrid-graphics laptops
// (NVIDIA Optimus / AMD PowerXpress). The vendor drivers read these exported
// symbols from the launching .exe at startup; a non-zero value routes the whole
// process to the high-performance GPU. Must be compiled into the executable's
// own translation unit (NOT a static lib, where the linker may drop them).
// Include this header from exactly ONE translation unit per executable.
#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
