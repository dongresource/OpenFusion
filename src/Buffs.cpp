#include "Buffs.hpp"

#include "PlayerManager.hpp"

#include <assert.h>
#include <set>

using namespace Buffs;

void Buff::onTick() {
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

void Buff::onExpire() {
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

void Buff::addStack(BuffStack* stack) {
    stacks.push_back(*stack);
}

BuffStack* Buff::getDominantBuff() {
    assert(!stacks.empty());

    BuffStack* dominant = nullptr;
    for(BuffStack& stack : stacks) {
        if(stack.buffClass > dominant->buffClass)
            dominant = &stack;
    }
    return dominant;
}

bool Buff::isStale() {
    return stacks.empty();
}

static void timeBuffUpdate(EntityRef self, BuffStack* buff, int type) {

    if(self.kind != EntityKind::PLAYER)
        return; // not implemented
    
    Player* plr = (Player*)self.getEntity();
    if(plr == nullptr)
        return;
    
    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
    pkt.eCSTB = buff->id; // eCharStatusTimeBuffID
    pkt.eTBU = type; // eTimeBuffUpdate
    pkt.eTBT = (buff->buffClass <= BuffClass::CONSUMABLE); // eTimeBuffType 1 means nano
    pkt.iConditionBitFlag = plr->getCompositeCondition();
    self.sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

void Buffs::timeBuffUpdateAdd(EntityRef self, BuffStack* buff) {
    timeBuffUpdate(self, buff, ETBU_ADD);
}

void Buffs::timeBuffUpdateDelete(EntityRef self, BuffStack* buff) {
    timeBuffUpdate(self, buff, ETBU_DEL);
}