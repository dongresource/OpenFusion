#include "Buffs.hpp"

#include "PlayerManager.hpp"

using namespace Buffs;

void Buff::tick() {
    auto it = stacks.begin();
    while(it != stacks.end()) {
        BuffStack& stack = *it;
        if(stack.onTick) stack.onTick(self, &stack);

        if(stack.durationTicks == 0) {
            // erase() destroys the callbacks
            // with the stack struct, so we need
            // to copy it first.
            BuffStack deadStack = stack;
            it = stacks.erase(it);
            if(deadStack.onExpire) deadStack.onExpire(self, &deadStack);
        } else {
            if(stack.durationTicks > 0) stack.durationTicks--;
            it++;
        }
    }
}

void Buff::clear() {
    while(!stacks.empty()) {
        BuffStack stack = stacks.back();
        stacks.pop_back();
        if(stack.onExpire) stack.onExpire(self, &stack);
    }
}

void Buff::addStack(BuffStack* stack) {
    BuffStack newStack = *stack;
    newStack.buff = this;
    if(newStack.onApply) newStack.onApply(self, &newStack);
    stacks.push_back(newStack);
}

bool Buff::hasClass(BuffClass buffClass) {
    for(BuffStack& stack : stacks) {
        if(stack.buffStackClass == buffClass)
            return true;
    }
    return false;
}

BuffClass Buff::maxClass() {
    BuffClass buffClass = BuffClass::NONE;
    for(BuffStack& stack : stacks) {
        if(stack.buffStackClass > buffClass)
            buffClass = stack.buffStackClass;
    }
    return buffClass;
}

bool Buff::isStale() {
    return stacks.empty();
}

#pragma region Handlers
void Buffs::timeBuffUpdate(EntityRef self, BuffStack* stack, int status) {

    if(self.kind != EntityKind::PLAYER)
        return; // not implemented
    
    Player* plr = (Player*)self.getEntity();
    if(plr == nullptr)
        return;

    if(status == ETBU_DEL && plr->hasBuff(stack->buff->id))
        return; // no premature status removal!

    sTimeBuff payload{};

    int cbf = plr->getCompositeCondition();
    if(status == ETBU_ADD) {
        if(stack->buff->id > 0) cbf |= CSB_FROM_ECSB(stack->buff->id);
        payload.iValue = stack->value;
        //payload.iTimeLimit = stack->durationTicks * MS_PER_PLAYER_TICK;
    }
    
    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
    pkt.eCSTB = stack->buff->id; // eCharStatusTimeBuffID
    pkt.eTBU = status; // eTimeBuffUpdate
    pkt.eTBT = (int)stack->buffStackClass;
    pkt.iConditionBitFlag = cbf;
    pkt.TimeBuff = payload;
    self.sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

void Buffs::timeBuffTimeoutViewable(EntityRef self, BuffStack* stack, int ct) {
    if(self.kind != EntityKind::PLAYER)
        return; // not implemented
    
    Player* plr = (Player*)self.getEntity();
    if(plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt); // send a buff timeout to other players
    pkt.eCT = ct; // 1 for eggs, at least
    pkt.iID = plr->iID;
    pkt.iConditionBitFlag = plr->getCompositeCondition();
    PlayerManager::sendToViewable(self.sock, pkt, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT);
}

/* MOVE TO EGG LAMBDA
void Buffs::timeBuffTimeout(EntityRef self, BuffStack* buff) {
    timeBuffUpdate(self, buff, ETBU_DEL);
    timeBuffTimeoutViewable(self);
} */
#pragma endregion
