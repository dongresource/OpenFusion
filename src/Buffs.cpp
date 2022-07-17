#include "Buffs.hpp"

#include "PlayerManager.hpp"

using namespace Buffs;

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