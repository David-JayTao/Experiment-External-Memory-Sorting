#ifndef REPLACEMENT_H
#define REPLACEMENT_H
#include "run_file.h"
#include <ostream>
// M个候选的败者树置换选择，B容量双输入/双输出，按阶段运行三个线程。
// 返回初始段目录下标，trace非空时输出冻结与同步过程。
std::vector<int> replacementSelection(const std::string& rawPath, RunFile& file,
                                      size_t memorySize, size_t blockSize, IoStats& stats,
                                      std::ostream* trace = 0);
#endif
