# GUMMY development
Keep public plugin IDs and parameter IDs stable. C++17, JUCE 8.0.6, CMake.
No allocations, I/O, locks, or GUI calls in audio callbacks. DSP lives under Source/DSP.
Run CTest after changes. Never claim AU/Logic validation from a Linux build.
