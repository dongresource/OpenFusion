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
    double scalingFactor = target->getMaxHP() / 1000.0;
    int healed = target->heal(sourceRef, heal * scalingFactor);

    sSkillResult_Heal_HP result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iHealHP = healed;
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
    PlayerManager::sendPlayerTo(source->getRef().sock, plr->recallX, plr->recallY, plr->recallZ, plr->recallInstance);

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
        case SkillType::DAMAGE:
            resultSize = sizeof(sSkillResult_Damage);
            skillHandler = handleSkillDamage;
            break;
        case SkillType::HEAL_HP:
        case SkillType::RETURNHOMEHEAL:
            resultSize = sizeof(sSkillResult_Heal_HP);
            skillHandler = handleSkillHealHP;
            break;
        case SkillType::JUMP:
        case SkillType::RUN:
        case SkillType::FREEDOM:
        case SkillType::PHOENIX:
        case SkillType::INVULNERABLE:
        case SkillType::MINIMAPENEMY:
        case SkillType::MINIMAPTRESURE:
        case SkillType::NANOSTIMPAK:
        case SkillType::PROTECTBATTERY:
        case SkillType::PROTECTINFECTION:
        case SkillType::REWARDBLOB:
        case SkillType::REWARDCASH:
        case SkillType::STAMINA_SELF:
        case SkillType::STEALTH:
            resultSize = sizeof(sSkillResult_Buff);
            skillHandler = handleSkillBuff;
            break;
        case SkillType::BATTERYDRAIN:
            resultSize = sizeof(sSkillResult_BatteryDrain);
            skillHandler = handleSkillBatteryDrain;
            break;
        case SkillType::RECALL: // still soft lock
        case SkillType::RECALL_GROUP: // works for player who uses it
            resultSize = sizeof(sSkillResult_Move);
            skillHandler = handleSkillMove;
            break;
        case SkillType::PHOENIX_GROUP: // broken
            resultSize = sizeof(sSkillResult_Resurrect);
            skillHandler = handleSkillResurrect;
            break;
        case SkillType::RETROROCKET_SELF:
            // no-op
            return results;
        default:
            std::cout << "[WARN] Unhandled skill type " << (int)skill->skillType << std::endl;
            return results;
    }

    for(ICombatant* target : targets) {
        assert(target != nullptr);
        SkillResult result = skillHandler(skill, power, src != nullptr ? src : target, target);
        if(result.size == 0) continue; // skill not applicable
        if(result.size != resultSize) {
            std::cout << "[WARN] bad skill result size for " << (int)skill->skillType << " from " << (void*)handleSkillBuff << std::endl;
            continue;
        }
        results.push_back(result);
    }
    return results;
}

static void attachSkillResults(std::vector<SkillResult> results, size_t resultSize, uint8_t* pivot) {
    for(SkillResult& result : results) {
        memcpy(pivot, result.payload, resultSize);
        pivot += resultSize;
    }
}

void Abilities::useNanoSkill(CNSocket* sock, SkillData* skill, sNano& nano, std::vector<ICombatant*> affected) {

    Player* plr = PlayerManager::getPlayer(sock);

    int boost = 0;
    if (Nanos::getNanoBoost(plr))
        boost = 3;

    nano.iStamina -= skill->batteryUse[boost];
    if (nano.iStamina < 0)
        nano.iStamina = 0;

    std::vector<SkillResult> results = handleSkill(skill, boost, plr, affected);
    size_t resultSize = 0; // guaranteed to be the same for every item
    if (!results.empty()) resultSize = results.back().size;

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
    pkt->eST = (int32_t)skill->skillType;
    pkt->iTargetCnt = (int32_t)results.size();

    attachSkillResults(results, resultSize, (uint8_t*)(pkt + 1));
    sock->sendPacket(pkt, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    PlayerManager::sendToViewable(sock, pkt, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);

    if (nano.iStamina <= 0)
        Nanos::summonNano(sock, -1);
}

void Abilities::useNPCSkill(EntityRef npc, int skillID, std::vector<ICombatant*> affected) {
    if(SkillTable.count(skillID) == 0)
        return;

    Entity* entity = npc.getEntity();
    ICombatant* src = nullptr;
    if(npc.kind == EntityKind::COMBAT_NPC || npc.kind == EntityKind::MOB)
        src = dynamic_cast<ICombatant*>(entity);
    
    SkillData* skill = &SkillTable[skillID];

    std::vector<SkillResult> results = handleSkill(skill, 0, src, affected);
    size_t resultSize = results.back().size; // guaranteed to be the same for every item

    if (!validOutVarPacket(sizeof(sP_FE2CL_NPC_SKILL_HIT), results.size(), resultSize)) {
        std::cout << "[WARN] bad sP_FE2CL_NPC_SKILL_HIT packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + results.size() * resultSize;
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_SKILL_HIT* pkt = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;
    pkt->iNPC_ID = npc.id;
    pkt->iSkillID = skillID;
    pkt->eST = (int32_t)skill->skillType;
    pkt->iTargetCnt = (int32_t)results.size();

    attachSkillResults(results, resultSize, (uint8_t*)(pkt + 1));
    NPCManager::sendToViewable(entity, pkt, P_FE2CL_NPC_SKILL_HIT, resplen);
}

static std::vector<ICombatant*> entityRefsToCombatants(std::vector<EntityRef> refs) {
    std::vector<ICombatant*> combatants;
    for(EntityRef ref : refs) {
        if(ref.kind == EntityKind::PLAYER)
            combatants.push_back(dynamic_cast<ICombatant*>(PlayerManager::getPlayer(ref.sock)));
        else if(ref.kind == EntityKind::COMBAT_NPC || ref.kind == EntityKind::MOB)
            combatants.push_back(dynamic_cast<ICombatant*>(ref.getEntity()));
    }
    return combatants;
}

std::vector<ICombatant*> Abilities::matchTargets(ICombatant* src, SkillData* skill, int count, int32_t *ids) {

    if(skill->effectTarget == SkillEffectTarget::SELF)
        return {src}; // client sends 0 targets for certain self-targeting skills (recall)

    if(skill->targetType == SkillTargetType::GROUP) {
        // group
        if(count != 1 || ids[0] != src->getID()) {
            std::cout << "[WARN] skill: bad group targeting (id " << ids[0] << ")\n";
            return {};
        }
        return entityRefsToCombatants(src->getGroupMembers());
    }

    // individuals
    std::vector<ICombatant*> targets;
    for (int i = 0; i < count; i++) {
        int32_t id = ids[i];
        if (skill->targetType == SkillTargetType::MOBS) {
            // mob
            if (NPCManager::NPCs.find(id) != NPCManager::NPCs.end()) {
                BaseNPC* npc = NPCManager::NPCs[id];
                if (npc->kind == EntityKind::COMBAT_NPC || npc->kind == EntityKind::MOB) {
                    targets.push_back(dynamic_cast<ICombatant*>(npc));
                    continue;
                }
            }
            std::cout << "[WARN] skill: invalid mob target (id " << id << ")\n";
        } else if(skill->targetType == SkillTargetType::PLAYERS) {
            // player
            Player* plr = PlayerManager::getPlayerFromID(id);
            if (plr != nullptr) {
                targets.push_back(dynamic_cast<ICombatant*>(plr));
                continue;
            }
            std::cout << "[WARN] skill: invalid player target (id " << id << ")\n";
        }
    }

    return targets; 
}

/* ripped from client (enums emplaced) */
int Abilities::getCSTBFromST(SkillType skillType) {
    int result = 0;
    switch (skillType)
    {
    case SkillType::RUN:
        result = ECSB_UP_MOVE_SPEED;
        break;
    case SkillType::JUMP:
        result = ECSB_UP_JUMP_HEIGHT;
        break;
    case SkillType::STEALTH:
        result = ECSB_UP_STEALTH;
        break;
    case SkillType::PHOENIX:
        result = ECSB_PHOENIX;
        break;
    case SkillType::PROTECTBATTERY:
        result = ECSB_PROTECT_BATTERY;
        break;
    case SkillType::PROTECTINFECTION:
        result = ECSB_PROTECT_INFECTION;
        break;
    case SkillType::SNARE:
        result = ECSB_DN_MOVE_SPEED;
        break;
    case SkillType::SLEEP:
        result = ECSB_MEZ;
        break;
    case SkillType::MINIMAPENEMY:
        result = ECSB_MINIMAP_ENEMY;
        break;
    case SkillType::MINIMAPTRESURE:
        result = ECSB_MINIMAP_TRESURE;
        break;
    case SkillType::REWARDBLOB:
        result = ECSB_REWARD_BLOB;
        break;
    case SkillType::REWARDCASH:
        result = ECSB_REWARD_CASH;
        break;
    case SkillType::INFECTIONDAMAGE:
        result = ECSB_INFECTION;
        break;
    case SkillType::FREEDOM:
        result = ECSB_FREEDOM;
        break;
    case SkillType::BOUNDINGBALL:
        result = ECSB_BOUNDINGBALL;
        break;
    case SkillType::INVULNERABLE:
        result = ECSB_INVULNERABLE;
        break;
    case SkillType::BUFFHEAL:
        result = ECSB_HEAL;
        break;
    case SkillType::NANOSTIMPAK:
        result = ECSB_STIMPAKSLOT1;
        break;
    }
    return result;
}
