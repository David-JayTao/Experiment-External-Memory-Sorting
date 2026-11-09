#ifndef REPLACEMENT_H
#define REPLACEMENT_H
#include "run_file.h"
#include <ostream>
std::vector<int> replacementSelection(const std::string& rawPath, RunFile& file,
                                      size_t memorySize, size_t blockSize, IoStats& stats,
                                      std::ostream* trace = 0);
#endif
