#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"

#include <vector>

/* forward declaration(s) */
struct BuffStack;

#define CSB_FROM_ECSB(x) (1 << (x - 1))

enum class BuffClass {
    NONE = ETBT_NONE,
	NANO = ETBT_NANO,
	GROUP_NANO = ETBT_GROUPNANO,
	EGG = ETBT_SHINY,
	ENVIRONMENT = ETBT_LANDEFFECT,
	ITEM = ETBT_ITEM,
	CASH_ITEM = ETBT_CASHITEM
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
    void onTick();
    void onExpire();
    void addStack(BuffStack* stack);

    /*
    * Why do this madness? Let me tell you why.
    * We need to be able to distinguish whether a player's
    * buff is from something like an egg vs. their nano,
    * because there are cases where the behavior is different.
    * Now, this is impossible to do when they all get composited
    * to a single bitfield. So we use a "buff class" and pick
    * the buff stack that is most "dominant".
    */
    BuffStack* getDominantBuff();

    /* 
     * In general, a Buff object won't exist
     * unless it has stacks. However, when
     * popping stacks during iteration (onExpire),
     * stacks will be empty for a brief moment
     * when the last stack is popped.
     */
    bool isStale();

    Buff(EntityRef pSelf, BuffStack* firstStack)
        : self(pSelf) {
        addStack(firstStack);
    }
};

namespace Buffs {
    void timeBuffUpdateAdd(EntityRef self, BuffStack* buff);
    void timeBuffUpdateDelete(EntityRef self, BuffStack* buff);
    void timeBuffTimeout(EntityRef self, BuffStack* buff);
}
