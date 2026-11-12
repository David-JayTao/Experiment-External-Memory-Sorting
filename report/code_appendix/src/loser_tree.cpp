#include "loser_tree.h"
#include <stdexcept>
#include <limits>
#include <utility>
// 将候选数补至2的幂后建树，额外叶子无效；空输入也保留一个无效叶。
LoserTree::LoserTree(const std::vector<Candidate>& candidates) : leafCount_(1), champion_(0) {
    if (candidates.size() > size_t(std::numeric_limits<int>::max() / 2))
        throw std::runtime_error("too many loser-tree leaves");
    while (size_t(leafCount_) < candidates.size()) leafCount_ *= 2;
    leaves_ = candidates;
    leaves_.resize(leafCount_);
    losers_.resize(leafCount_, -1);
    champion_ = build(1);
}
// 无效值不用 INT_MAX 作哨兵，因而真实 INT_MAX 可以正常参与排序。
// 比较叶下标：有效优先、未冻结优先、值小优先、下标小优先。
bool LoserTree::less(int left, int right) const {
    const Candidate& a = leaves_[left];
    const Candidate& b = leaves_[right];
    if (a.valid != b.valid) return a.valid;
    if (!a.valid) return left < right;
    if (a.frozen != b.frozen) return !a.frozen;
    if (a.value != b.value) return a.value < b.value;
    return left < right;
}
// 递归比赛：返回子树冠军，内部节点只保存败者的叶下标。
int LoserTree::build(int node) {
    if (node >= leafCount_) return node - leafCount_;
    int left = build(node * 2), right = build(node * 2 + 1);
    if (less(right, left)) std::swap(left, right);
    losers_[node] = right;
    return left;
}
// 返回有效冠军的叶下标；所有叶无效时返回-1。
int LoserTree::winner() const { return leaves_[champion_].valid ? champion_ : -1; }
// 查看冠军候选；调用者先用winner判断是否仍有有效元素。
const Candidate& LoserTree::top() const { return leaves_[champion_]; }
// 用新候选替换当前冠军，仅沿该叶到根的路径重赛，时间O(log M)。
void LoserTree::replaceWinner(const Candidate& candidate) {
    if (winner() < 0) throw std::runtime_error("cannot replace an empty tree winner");
    int challenger = champion_;
    leaves_[challenger] = candidate;
    // 原冠军沿途击败的对手恰好保存在这些节点，不需要重算其他子树。
    for (int node = (leafCount_ + challenger) / 2; node > 0; node /= 2) {
        if (less(losers_[node], challenger)) std::swap(challenger, losers_[node]);
    }
    champion_ = challenger;
}
// 开始新段：清除冻结位并重建比赛；有效性不变，因此不会复活EOF叶。
void LoserTree::thaw() {
    for (size_t index = 0; index < leaves_.size(); ++index) leaves_[index].frozen = false;
    champion_ = build(1);
}
// 输出冠军、各内部败者和有效叶；冻结标记用于课堂跟踪段边界。
void LoserTree::print(std::ostream& output) const {
    output << "champion=" << winner() << " losers:";
    for (int node = 1; node < leafCount_; ++node) output << ' ' << node << ':' << losers_[node];
    output << " leaves:";
    for (size_t index = 0; index < leaves_.size(); ++index)
        if (leaves_[index].valid) output << ' ' << index << '=' << leaves_[index].value << (leaves_[index].frozen ? "(frozen)" : "");
    output << '\n';
}
