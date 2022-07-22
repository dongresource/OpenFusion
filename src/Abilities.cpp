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

/* ripped from client (enums emplaced) */
int Abilities::getCSTBFromST(int eSkillType) {
    int result = 0;
    switch (eSkillType)
    {
    case EST_RUN:
        result = ECSB_UP_MOVE_SPEED;
        break;
    case EST_JUMP:
        result = ECSB_UP_JUMP_HEIGHT;
        break;
    case EST_STEALTH:
        result = ECSB_UP_STEALTH;
        break;
    case EST_PHOENIX:
        result = ECSB_PHOENIX;
        break;
    case EST_PROTECTBATTERY:
        result = ECSB_PROTECT_BATTERY;
        break;
    case EST_PROTECTINFECTION:
        result = ECSB_PROTECT_INFECTION;
        break;
    case EST_SNARE:
        result = ECSB_DN_MOVE_SPEED;
        break;
    case EST_SLEEP:
        result = ECSB_MEZ;
        break;
    case EST_MINIMAPENEMY:
        result = ECSB_MINIMAP_ENEMY;
        break;
    case EST_MINIMAPTRESURE:
        result = ECSB_MINIMAP_TRESURE;
        break;
    case EST_REWARDBLOB:
        result = ECSB_REWARD_BLOB;
        break;
    case EST_REWARDCASH:
        result = ECSB_REWARD_CASH;
        break;
    case EST_INFECTIONDAMAGE:
        result = ECSB_INFECTION;
        break;
    case EST_FREEDOM:
        result = ECSB_FREEDOM;
        break;
    case EST_BOUNDINGBALL:
        result = ECSB_BOUNDINGBALL;
        break;
    case EST_INVULNERABLE:
        result = ECSB_INVULNERABLE;
        break;
    case EST_BUFFHEAL:
        result = ECSB_HEAL;
        break;
    case EST_NANOSTIMPAK:
        result = ECSB_STIMPAKSLOT1;
        break;
    }
    return result;
}

#pragma region Skill Handlers
bool Abilities::usePassiveNanoSkill(SkillData* skill, Player* plr, int boost) {
    assert(skill->drainType == SkillDrainType::PASSIVE);

    bool newBuffApplied = false;

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

    int timeBuffId = getCSTBFromST(skill->skillType);
    int value = skill->values[0][boost];

    BuffStack passiveBuff = {
        1, // passive nano buffs refreshed every tick
        value,
        self,
        BuffClass::NONE, // overwritten per target
    };

    for (EntityRef target : targets) {
        Entity* entity = target.getEntity();
        if (entity->kind != PLAYER && entity->kind != COMBAT_NPC && entity->kind != MOB)
            continue; // not a combatant

        passiveBuff.buffStackClass = target == self ? BuffClass::NANO : BuffClass::GROUP_NANO;
        ICombatant* combatant = dynamic_cast<ICombatant*>(entity);
        newBuffApplied |= combatant->addBuff(timeBuffId,
            [](EntityRef self, Buff* buff, int status, BuffStack* stack) {
                Buffs::timeBuffUpdate(self, buff, status, stack);
            },
            [](EntityRef self, Buff* buff, time_t currTime) {
                // TODO time buff tick
            },
            &passiveBuff);
    }

    return newBuffApplied;
}
#pragma endregion

void Abilities::init() {
    //REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
