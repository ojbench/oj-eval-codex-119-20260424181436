// Linear Scan Register Allocation implementation
// The OJ will include this file as "src.hpp"

#ifndef SRC_HPP
#define SRC_HPP

#include <string>
#include <vector>
#include <set>

class Location {
public:
    virtual std::string show() const = 0;
    virtual int getId() const = 0;
    virtual ~Location() = default;
};

class Register : public Location {
private:
    int id_;
public:
    explicit Register(int regId) : id_(regId) {}
    std::string show() const override { return std::string("reg") + std::to_string(id_); }
    int getId() const override { return id_; }
};

class StackSlot : public Location {
public:
    StackSlot() {}
    std::string show() const override { return std::string("stack"); }
    int getId() const override { return -1; }
};

struct LiveInterval {
    int startpoint;
    int endpoint;
    Location* location = nullptr;
};

class LinearScanRegisterAllocator {
private:
    // Active set ordered by endpoint ascending; stores indices into interval list
    struct ActiveCmp {
        const std::vector<LiveInterval>* p;
        bool operator()(int a, int b) const {
            const auto &A = (*p)[a];
            const auto &B = (*p)[b];
            if (A.endpoint != B.endpoint) return A.endpoint < B.endpoint;
            return a < b; // tie-break for strict weak ordering
        }
    };

    int regNum_;
    // Free register stack: FILO. We maintain as a vector used as stack.
    std::vector<int> freeRegs_;

    // Helper to push a freed register following FILO rule
    void pushFreeReg(int r) { freeRegs_.push_back(r); }

    // Expire intervals whose endpoint < i.startpoint; free their registers
    void expireOldIntervals(std::set<int, ActiveCmp>& active,
                            const std::vector<LiveInterval>& intervals,
                            int iIdx) {
        // iterate from smallest endpoint upward while they end before current start
        auto it = active.begin();
        while (it != active.end()) {
            int jIdx = *it;
            if (intervals[jIdx].endpoint >= intervals[iIdx].startpoint) break;
            // free register if any
            if (intervals[jIdx].location && intervals[jIdx].location->getId() >= 0) {
                pushFreeReg(intervals[jIdx].location->getId());
            }
            it = active.erase(it);
        }
    }

    // Spill at interval: choose the interval with the largest endpoint among active∪{i}
    void spillAtInterval(std::set<int, ActiveCmp>& active,
                         std::vector<LiveInterval>& intervals,
                         int iIdx) {
        // candidate is the one with largest endpoint in active
        int spillIdx = iIdx;
        if (!active.empty()) {
            auto itLast = active.end();
            --itLast; // largest endpoint due to ascending order
            int lastIdx = *itLast;
            if (intervals[lastIdx].endpoint > intervals[iIdx].endpoint) {
                // spill lastIdx, give its reg to i
                spillIdx = lastIdx;
                // move register from lastIdx to iIdx
                intervals[iIdx].location = intervals[lastIdx].location;
                intervals[lastIdx].location = new StackSlot();
                // replace lastIdx with iIdx in active (maintaining order by set ops)
                active.erase(itLast);
                active.insert(iIdx);
                return;
            }
        }
        // Otherwise spill i
        intervals[iIdx].location = new StackSlot();
    }

public:
    explicit LinearScanRegisterAllocator(int regNum) : regNum_(regNum) {
        // initialize free register stack with 0..regNum-1 such that FILO prefers most recently freed;
        // initially none were used before; instruction says prefer smallest id first in initial state.
        // To achieve that with a stack, push in descending order so the first pop gives 0.
        for (int r = regNum_ - 1; r >= 0; --r) freeRegs_.push_back(r);
    }

    void linearScanRegisterAllocate(std::vector<LiveInterval>& intervalList) {
        // active set with comparator bound to current interval list
        ActiveCmp cmp{&intervalList};
        std::set<int, ActiveCmp> active(cmp);

        const int n = static_cast<int>(intervalList.size());
        for (int i = 0; i < n; ++i) {
            // expire old intervals
            expireOldIntervals(active, intervalList, i);

            if (!freeRegs_.empty()) {
                int r = freeRegs_.back();
                freeRegs_.pop_back();
                intervalList[i].location = new Register(r);
                active.insert(i);
            } else {
                spillAtInterval(active, intervalList, i);
                // only insert if not spilled (i has a register)
                if (intervalList[i].location && intervalList[i].location->getId() >= 0) {
                    active.insert(i);
                }
            }
        }
    }
};

#endif // SRC_HPP

