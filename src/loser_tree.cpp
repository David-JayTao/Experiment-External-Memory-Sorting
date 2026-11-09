#include "loser_tree.h"
#include <stdexcept>
#include <utility>
LoserTree::LoserTree(const std::vector<Candidate>& candidates) : leafCount_(1), champion_(0) {
    while (size_t(leafCount_) < candidates.size()) leafCount_ *= 2;
    leaves_ = candidates;
    leaves_.resize(leafCount_);
    losers_.resize(leafCount_, -1);
    champion_ = build(1);
}
// 无效值不用 INT_MAX 作哨兵，因而真实 INT_MAX 可以正常参与排序。
bool LoserTree::less(int left, int right) const {
    const Candidate& a = leaves_[left];
    const Candidate& b = leaves_[right];
    if (a.valid != b.valid) return a.valid;
    if (!a.valid) return left < right;
    if (a.frozen != b.frozen) return !a.frozen;
    if (a.value != b.value) return a.value < b.value;
    return left < right;
}
int LoserTree::build(int node) {
    if (node >= leafCount_) return node - leafCount_;
    int left = build(node * 2), right = build(node * 2 + 1);
    if (less(right, left)) std::swap(left, right);
    losers_[node] = right;
    return left;
}
int LoserTree::winner() const { return leaves_[champion_].valid ? champion_ : -1; }
const Candidate& LoserTree::top() const { return leaves_[champion_]; }
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
void LoserTree::thaw() {
    for (size_t index = 0; index < leaves_.size(); ++index) leaves_[index].frozen = false;
    champion_ = build(1);
}
void LoserTree::print(std::ostream& output) const {
    output << "champion=" << winner() << " losers:";
    for (int node = 1; node < leafCount_; ++node) output << ' ' << node << ':' << losers_[node];
    output << " leaves:";
    for (size_t index = 0; index < leaves_.size(); ++index)
        if (leaves_[index].valid) output << ' ' << index << '=' << leaves_[index].value << (leaves_[index].frozen ? "(frozen)" : "");
    output << '\n';
}
