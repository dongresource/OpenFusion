#include "Abilities.hpp"

#include "NPCManager.hpp"
#include "PlayerManager.hpp"
#include "Buffs.hpp"

#include <assert.h>

using namespace Abilities;

std::map<int32_t, SkillData> Abilities::SkillTable;

/*
// New email notification
static void emailUpdateCheck(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_REP_PC_NEW_EMAIL, resp);
    resp.iNewEmailCnt = Database::getUnreadEmailCount(PlayerManager::getPlayer(sock)->iID);
    sock->sendPacket(resp, P_FE2CL_REP_PC_NEW_EMAIL);
}
*/

std::vector<EntityRef> Abilities::matchTargets(SkillData* skill, int count, int32_t *ids) {

    std::vector<EntityRef> targets;

    for (int i = 0; i < count; i++) {
        int32_t id = ids[i];
        if (skill->targetType == SkillTargetType::MOBS) {
            // mob?
            if (NPCManager::NPCs.find(id) != NPCManager::NPCs.end()) targets.push_back(id);
            else std::cout << "[WARN] skill: id not found\n";
        } else {
            // player?
            CNSocket* sock = PlayerManager::getSockFromID(id);
            if (sock != nullptr) targets.push_back(sock);
            else std::cout << "[WARN] skill: sock not found\n";
        }
    }

    return targets; 
}

/* ripped from client */
int Abilities::getCSTBFromST(int eSkillType) {
    int result = 0;
    switch (eSkillType)
    {
    case 11:
        result = 1;
        break;
    case 10:
        result = 3;
        break;
    case 12:
        result = 4;
        break;
    case 16:
        result = 5;
        break;
    case 17:
        result = 6;
        break;
    case 18:
        result = 7;
        break;
    case 5:
        result = 8;
        break;
    case 4:
        result = 11;
        break;
    case 14:
        result = 13;
        break;
    case 15:
        result = 14;
        break;
    case 19:
        result = 15;
        break;
    case 20:
        result = 16;
        break;
    case 23:
        result = 17;
        break;
    case 25:
        result = 18;
        break;
    case 31:
        result = 19;
        break;
    case 32:
        result = 20;
        break;
    case 35:
        result = 24;
        break;
    case 33:
        result = 21;
        break;
    }
    return result;
}

#pragma region Skill Handlers
void Abilities::usePassiveNanoSkill(SkillData* skill, Player* plr) {
    assert(skill->drainType == SkillDrainType::PASSIVE);

    EntityRef self = PlayerManager::getSockFromID(plr->iID);
    std::vector<EntityRef> targets;
    if (skill->targetType == SkillTargetType::GROUP) {
        targets = plr->getGroupMembers(); // group
    }
    else if(skill->targetType == SkillTargetType::SELF) {
        targets.push_back(self); // self
    } else {
        std::cout << "[WARN] Passive skill with type " << skill->skillType << " has target type MOB" << std::endl;
    }

    BuffStack passiveBuff = {
        1, // passive nano buffs refreshed every tick
        self,
        BuffClass::NONE, // overwritten per target
        [](EntityRef host, BuffStack* stack)  {
            Buffs::timeBuffUpdate(host, stack, ETBU_ADD);
        },
        nullptr,
        [](EntityRef host, BuffStack* stack)  {
            Buffs::timeBuffUpdate(host, stack, ETBU_DEL);
        }
    };

    for (EntityRef target : targets) {
        Entity* entity = target.getEntity();
        if (entity->kind != PLAYER && entity->kind != COMBAT_NPC && entity->kind != MOB)
            continue; // not a combatant

        passiveBuff.buffStackClass = target == self ? BuffClass::NANO : BuffClass::GROUP_NANO;
        ICombatant* combatant = dynamic_cast<ICombatant*>(entity);
        combatant->addBuff(getCSTBFromST(skill->skillType), &passiveBuff);
    }
}
#pragma endregion

void Abilities::init() {
    //REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
