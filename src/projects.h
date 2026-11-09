#ifndef PROJECTS_H
#define PROJECTS_H
#include "merge_plan.h"
struct SortResult {
    int finalRun = -1;
    uint64_t inputCount = 0;
    size_t initialRuns = 0;
    uint64_t ordinaryCost = 0;
    uint64_t optimalCost = 0;
    IoStats io;
    double milliseconds = 0;
};
SortResult sortFile(int project, const std::string& rawPath, const std::string& runPath,
                    size_t memorySize, size_t blockSize, size_t ways, std::ostream* trace = 0);
void printResult(const SortResult& result, std::ostream& output);
#endif
