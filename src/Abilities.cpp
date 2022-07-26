#include "Abilities.hpp"

#include "NPCManager.hpp"
#include "PlayerManager.hpp"
#include "Buffs.hpp"
#include "Nanos.hpp"

#include <assert.h>

using namespace Abilities;

std::map<int32_t, SkillData> Abilities::SkillTable;

#pragma region Skill handlers
static SkillResult handleSkillDamage(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    EntityRef sourceRef = source->getRef();
    double scalingFactor = 1;
    if(sourceRef.kind == EntityKind::PLAYER)
        scalingFactor = std::max(source->getMaxHP(), target->getMaxHP()) / 1000.0;
    else
        scalingFactor = source->getMaxHP() / 1500.0;

    int damage = (int)(skill->values[0][power] * scalingFactor);
    int dealt = target->takeDamage(sourceRef, damage);

    sSkillResult_Damage result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = dealt <= 0;
    result.iDamage = dealt;
    result.iHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Damage), &result);
}

static SkillResult handleSkillHealHP(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    EntityRef sourceRef = source->getRef();
    int heal = skill->values[0][power];
    int healed = target->heal(sourceRef, heal);

    sSkillResult_Heal_HP result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iHealHP = heal;
    result.iHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Heal_HP), &result);
}

static SkillResult handleSkillDamageNDebuff(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    // TODO abilities
    sSkillResult_Damage_N_Debuff result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = false;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_Damage_N_Debuff), &result);
}

static SkillResult handleSkillBuff(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    sSkillResult_Buff result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = false;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_Buff), &result);
}

static SkillResult handleSkillBatteryDrain(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    if(target->getCharType() != 1)
        return SkillResult(); // only Players are valid targets for battery drain
    Player* plr = dynamic_cast<Player*>(target);

    const double scalingFactor = (18 + source->getLevel()) / 36.0;

    int boostDrain = (int)(skill->values[0][power] * scalingFactor);
    if(boostDrain > plr->batteryW) boostDrain = plr->batteryW;
    plr->batteryW -= boostDrain;

    int potionDrain = (int)(skill->values[1][power] * scalingFactor);
    if(potionDrain > plr->batteryN) potionDrain = plr->batteryN;
    plr->batteryN -= potionDrain;

    sSkillResult_BatteryDrain result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = target->hasBuff(ECSB_PROTECT_BATTERY);
    result.iDrainW = boostDrain;
    result.iBatteryW = plr->batteryW;
    result.iDrainN = potionDrain;
    result.iBatteryN = plr->batteryN;
    result.iStamina = plr->getActiveNano()->iStamina;
    result.bNanoDeactive = plr->getActiveNano()->iStamina <= 0;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_BatteryDrain), &result);
}

static SkillResult handleSkillMove(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    if(source->getCharType() != 1)
        return SkillResult(); // only Players are valid sources for recall
    Player* plr = dynamic_cast<Player*>(source);

    sSkillResult_Move result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iMapNum = plr->recallInstance;
    result.iMoveX = plr->recallX;
    result.iMoveY = plr->recallY;
    result.iMoveZ = plr->recallZ;
    return SkillResult(sizeof(sSkillResult_Move), &result);
}

static SkillResult handleSkillResurrect(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    sSkillResult_Resurrect result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iRegenHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Resurrect), &result);
}
#pragma endregion

static std::vector<SkillResult> handleSkill(SkillData* skill, int power, ICombatant* src, std::vector<ICombatant*> targets) {
    size_t resultSize = 0;
    SkillResult (*skillHandler)(SkillData*, int, ICombatant*, ICombatant*) = nullptr;
    std::vector<SkillResult> results;

    switch(skill->skillType)
    {
        case EST_DAMAGE:
            resultSize = sizeof(sSkillResult_Damage);
            skillHandler = handleSkillDamage;
            break;
        case EST_HEAL_HP:
        case EST_RETURNHOMEHEAL:
            resultSize = sizeof(sSkillResult_Heal_HP);
            skillHandler = handleSkillHealHP;
            break;
        case EST_JUMP:
        case EST_RUN:
        case EST_FREEDOM:
        case EST_PHOENIX:
        case EST_INVULNERABLE:
        case EST_MINIMAPENEMY:
        case EST_MINIMAPTRESURE:
        case EST_NANOSTIMPAK:
        case EST_PROTECTBATTERY:
        case EST_PROTECTINFECTION:
        case EST_REWARDBLOB:
        case EST_REWARDCASH:
        case EST_STAMINA_SELF:
        case EST_STEALTH:
            resultSize = sizeof(sSkillResult_Buff);
            skillHandler = handleSkillBuff;
            break;
        case EST_BATTERYDRAIN:
            resultSize = sizeof(sSkillResult_BatteryDrain);
            skillHandler = handleSkillBatteryDrain;
            break;
        case EST_RECALL:
        case EST_RECALL_GROUP:
            resultSize = sizeof(sSkillResult_Move);
            skillHandler = handleSkillMove;
            break;
        case EST_PHOENIX_GROUP:
            resultSize = sizeof(sSkillResult_Resurrect);
            skillHandler = handleSkillResurrect;
            break;
        default:
            std::cout << "[WARN] Unhandled skill type " << skill->skillType << std::endl;
            return results;
    }
    assert(skillHandler != nullptr);

    for(ICombatant* target : targets) {
        SkillResult result = skillHandler(skill, power, src, target);
        if(result.size == 0) continue; // skill not applicable
        if(result.size != resultSize) {
            std::cout << "[WARN] bad skill result size for " << skill->skillType << " from " << (void*)handleSkillBuff << std::endl;
            continue;
        }
        results.push_back(result);
    }
    return results;
}

void Abilities::broadcastNanoSkill(CNSocket* sock, sNano& nano, std::vector<ICombatant*> affected) {
    if(SkillTable.count(nano.iSkillID) == 0)
        return;

    SkillData* skill = &SkillTable[nano.iSkillID];
    Player* plr = PlayerManager::getPlayer(sock);

    std::vector<SkillResult> results = handleSkill(skill, Nanos::getNanoBoost(plr), plr, affected);
    size_t resultSize = results.back().size; // guaranteed to be the same for every item

    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), results.size(), resultSize)) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + results.size() * resultSize;
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE_SUCC* pkt = (sP_FE2CL_NANO_SKILL_USE_SUCC*)respbuf;
    pkt->iPC_ID = plr->iID;
    pkt->iNanoID = nano.iID;
    pkt->iSkillID = nano.iSkillID;
    pkt->iNanoStamina = nano.iStamina;
    pkt->bNanoDeactive = nano.iStamina <= 0;
    pkt->eST = skill->skillType;
    pkt->iTargetCnt = (int32_t)results.size();

    uint8_t* appended = (uint8_t*)(pkt + 1);
    for(SkillResult& result : results) {
        memcpy(appended, result.payload, resultSize);
        appended += resultSize;
    }

    sock->sendPacket(pkt, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    PlayerManager::sendToViewable(sock, pkt, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
}

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

std::vector<ICombatant*> Abilities::usePassiveNanoSkill(SkillData* skill, Player* plr) {
    assert(skill->drainType == SkillDrainType::PASSIVE);

    EntityRef self = PlayerManager::getSockFromID(plr->iID);
    std::vector<ICombatant*> affected;
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
    int boost = Nanos::getNanoBoost(plr) ? 3 : 0;
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
        if(combatant->addBuff(timeBuffId,
            [](EntityRef self, Buff* buff, int status, BuffStack* stack) {
                Buffs::timeBuffUpdate(self, buff, status, stack);
            },
            [](EntityRef self, Buff* buff, time_t currTime) {
                // TODO time buff tick
            },
            &passiveBuff)) affected.push_back(combatant);
    }

    return affected;
}

void Abilities::init() {
    //REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
