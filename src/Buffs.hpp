#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"

#include <vector>
#include <functional>

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

typedef std::function<void(EntityRef, BuffStack*)> BuffCallback;

struct BuffStack {
    int durationTicks;
    EntityRef source;
    BuffClass buffStackClass;

    /* called just before the stack is added */
    BuffCallback onApply;

    /* called when the stack is ticked */
    BuffCallback onTick;

    /* called just after the stack is removed */
    BuffCallback onExpire;

    Buff* buff;
};

class Buff {
private:
    EntityRef self;
    std::vector<BuffStack> stacks;

public:
    int id;

    void tick();
    void clear();
    void addStack(BuffStack* stack);

    /*
    * Sometimes we need to determine if a buff
    * is covered by a certain class, ex: nano
    * vs. coco egg in the case of infection protection
    */
    bool hasClass(BuffClass buffClass);
    BuffClass maxClass();

    /* 
     * In general, a Buff object won't exist
     * unless it has stacks. However, when
     * popping stacks during iteration (onExpire),
     * stacks will be empty for a brief moment
     * when the last stack is popped.
     */
    bool isStale();

    Buff(int iid, EntityRef pSelf, BuffStack* firstStack)
        : id(iid), self(pSelf) {
        addStack(firstStack);
    }
};

namespace Buffs {
    void timeBuffUpdate(EntityRef self, BuffStack* stack, int status);
    void timeBuffTimeoutViewable(EntityRef self, BuffStack* stack, int ct);
}
