#ifndef LOSER_TREE_H
#define LOSER_TREE_H
#include <cstdint>
#include <vector>
#include <ostream>
struct Candidate {
    int32_t value = 0;
    bool valid = false;            // false 表示输入结束或补齐叶子
    bool frozen = false;           // true 表示只属于下一段
};
// 内部节点只存败者；总冠军单独存放。更新限定为替换当前冠军。
class LoserTree {
public:
    explicit LoserTree(const std::vector<Candidate>& candidates);
    int winner() const;
    const Candidate& top() const;
    void replaceWinner(const Candidate& candidate);
    void thaw();
    void print(std::ostream& output) const;
private:
    std::vector<Candidate> leaves_; // 补齐到 2 的幂，额外叶子无效
    std::vector<int> losers_;       // losers_[1..leafCount_-1] 是各场败者的叶下标
    int leafCount_;
    int champion_;
    bool less(int left, int right) const;
    int build(int node);            // 返回该子树冠军，并保存内部败者
};
#endif
