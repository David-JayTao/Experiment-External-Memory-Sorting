#ifndef LOSER_TREE_H
#define LOSER_TREE_H
#include <cstdint>
#include <vector>
#include <ostream>
// 败者树叶子：数值与状态分开，完整支持int32极值。
struct Candidate {
    int32_t value = 0; // 有效时参与比较的整数
    bool valid = false;            // false 表示输入结束或补齐叶子
    bool frozen = false;           // true 表示只属于下一段
};
// 内部节点只存败者；总冠军单独存放。更新限定为替换当前冠军。
class LoserTree {
public:
    explicit LoserTree(const std::vector<Candidate>& candidates);
    int winner() const;
    const Candidate& top() const;
    // 只替换当前冠军并沿路径重赛；可传无效候选表示该叶输入结束。
    void replaceWinner(const Candidate& candidate);
    // 开始下一段时清除冻结状态并重建树。
    void thaw();
    void print(std::ostream& output) const;
private:
    std::vector<Candidate> leaves_; // 补齐到 2 的幂，额外叶子无效
    std::vector<int> losers_;       // losers_[1..leafCount_-1] 是各场败者的叶下标
    int leafCount_; // 补齐后的叶子数量
    int champion_; // 当前总冠军叶下标；有效性由叶状态判断
    bool less(int left, int right) const;
    int build(int node);            // 返回该子树冠军，并保存内部败者
};
#endif
