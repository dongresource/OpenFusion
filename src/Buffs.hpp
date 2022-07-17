#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"

#include <assert.h>
#include <vector>
#include <set>

/* forward declaration(s) */
struct BuffStack;

#define CSB_FROM_ECSB(x) (1 << (x - 1))

enum class BuffClass {
    NANO = 0,
    CONSUMABLE,
    EGG,
    OTHER
};

typedef void (*BuffCallback)(EntityRef, BuffStack*);

struct BuffStack {
    int id; // ECSB
    int durationTicks;
    EntityRef source;
    BuffClass buffClass;

    BuffCallback onApply;
    BuffCallback onTick;
    BuffCallback onExpire;
};

class Buff {
private:
    EntityRef self;
    std::vector<BuffStack> stacks;

public:
    void onTick() {
        assert(!stacks.empty());

        std::set<BuffCallback> callbacks;
        auto it = stacks.begin();
        while(it != stacks.end()) {
            BuffStack& stack = *it;
            if(stack.onTick != nullptr && callbacks.count(stack.onTick) == 0) {
                // unique callback
                stack.onTick(self, &stack);
                callbacks.insert(stack.onTick);
            }

            if(stack.durationTicks > 0) stack.durationTicks--;
            if(stack.durationTicks == 0) {
                it = stacks.erase(it);
                stack.onExpire(self, &stack);
            } else it++;
        }
    }

    void onExpire() {
        assert(!stacks.empty());

        std::set<BuffCallback> callbacks;
        while(!stacks.empty()) {
            BuffStack stack = stacks.back();
            stacks.pop_back();
            if(stack.onExpire != nullptr && callbacks.count(stack.onExpire) == 0) {
                // execute unique callback
                callbacks.insert(stack.onExpire);
                stack.onExpire(self, &stack);
            }
        }
    }

    void addStack(BuffStack* stack) {
        stacks.push_back(*stack);
    }

    /*
    * Why do this madness? Let me tell you why.
    * We need to be able to distinguish whether a player's
    * buff is from something like an egg vs. their nano,
    * because there are cases where the behavior is different.
    * Now, this is impossible to do when they all get composited
    * to a single bitfield. So we use a "buff class" and pick
    * the buff stack that is most "dominant".
    */
    BuffStack* getDominantBuff() {
        assert(!stacks.empty());

        BuffStack* dominant = nullptr;
        for(BuffStack& stack : stacks) {
            if(stack.buffClass > dominant->buffClass)
                dominant = &stack;
        }
        return dominant;
    }

    /* 
     * In general, a Buff object won't exist
     * unless it has stacks. However, when
     * popping stacks during iteration (onExpire),
     * stacks will be empty for a brief moment
     * when the last stack is popped.
     */
    bool isStale() {
        return stacks.empty();
    }

    Buff(EntityRef pSelf, BuffStack* firstStack)
        : self(pSelf) {
        addStack(firstStack);
    }
};

namespace Buffs {
    void timeBuffUpdateAdd(EntityRef self, BuffStack* buff);
    void timeBuffUpdateDelete(EntityRef self, BuffStack* buff);
}
