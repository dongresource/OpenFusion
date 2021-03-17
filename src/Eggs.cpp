#include "OpenFusion.hpp"

#include <assert.h>

using namespace Eggs;

/// sock, CBFlag -> until
std::map<std::pair<CNSocket*, int32_t>, time_t> Eggs::EggBuffs;
std::unordered_map<int, EggType> Eggs::EggTypes;
std::unordered_map<int, Egg*> Eggs::Eggs;

int Eggs::eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration) {
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    int bitFlag = Groups::getGroupFlags(otherPlr);
    int CBFlag = Nanos::applyBuff(sock, skillId, 1, 3, bitFlag);

    size_t resplen; 

    if (skillId == 183) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Damage);
    } else if (skillId == 150) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Heal_HP);
    } else {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Buff);
    }
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    sP_FE2CL_NPC_SKILL_HIT* skillUse = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;

    if (skillId == 183) { // damage egg
        sSkillResult_Damage* skill = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iDamage = PC_MAXHEALTH(plr->level) * Nanos::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP -= skill->iDamage;
        if (plr->HP < 0)
            plr->HP = 0;
        skill->iHP = plr->HP;
    } else if (skillId == 150) { // heal egg
        sSkillResult_Heal_HP* skill = (sSkillResult_Heal_HP*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iHealHP = PC_MAXHEALTH(plr->level) * Nanos::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP += skill->iHealHP;
        if (plr->HP > PC_MAXHEALTH(plr->level))
            plr->HP = PC_MAXHEALTH(plr->level);
        skill->iHP = plr->HP;
    } else { // regular buff egg
        sSkillResult_Buff* skill = (sSkillResult_Buff*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iConditionBitFlag = plr->iConditionBitFlag;
    }

    skillUse->iNPC_ID = eggId;
    skillUse->iSkillID = skillId;
    skillUse->eST = Nanos::SkillTable[skillId].skillType;
    skillUse->iTargetCnt = 1;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);

    if (CBFlag == 0)
        return -1;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, CBFlag);

    // save the buff serverside;
    // if you get the same buff again, new duration will override the previous one
    time_t until = getTime() + (time_t)duration * 1000;
    EggBuffs[key] = until;

    return 0;
}

static void eggStep(CNServer* serv, time_t currTime) {
    // tick buffs
    time_t timeStamp = currTime;
    auto it = EggBuffs.begin();
    while (it != EggBuffs.end()) {
        // check remaining time
        if (it->second > timeStamp)
            it++;
        else { // if time reached 0
            CNSocket* sock = it->first.first;
            int32_t CBFlag = it->first.second;
            Player* plr = PlayerManager::getPlayer(sock);
            Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            int groupFlags = Groups::getGroupFlags(otherPlr);
            for (auto& pwr : Nanos::NanoPowers) {
                if (pwr.bitFlag == CBFlag) { // pick the power with the right flag and unbuff
                    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
                    resp.eCSTB = pwr.timeBuffID;
                    resp.eTBU = 2;
                    resp.eTBT = 3; // for egg buffs
                    plr->iConditionBitFlag &= ~CBFlag;
                    resp.iConditionBitFlag = plr->iConditionBitFlag |= groupFlags | plr->iSelfConditionBitFlag;
                    sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

                    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, resp2); // send a buff timeout to other players
                    resp2.eCT = 1;
                    resp2.iID = plr->iID;
                    resp2.iConditionBitFlag = plr->iConditionBitFlag;
                    PlayerManager::sendToViewable(sock, (void*)&resp2, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
                }
            }
            // remove buff from the map
            it = EggBuffs.erase(it);
        }
    }

    // check dead eggs and eggs in inactive chunks
    for (auto egg : Eggs::Eggs) {
        if (!egg.second->dead || !Chunking::inPopulatedChunks(egg.second->viewableChunks))
            continue;
        if (egg.second->deadUntil <= timeStamp) {
            // respawn it
            egg.second->dead = false;
            egg.second->deadUntil = 0;
            egg.second->appearanceData.iHP = 400;
            
            Chunking::addNPCToChunks(Chunking::getViewableChunks(egg.second->chunkPos), egg.first);
        }
    }

}

void Eggs::npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg) {
    egg->iX = npc->iX;
    egg->iY = npc->iY;
    egg->iZ = npc->iZ;
    // client doesn't care about egg->iMapNum
    egg->iShinyType = npc->iNPCType;
    egg->iShiny_ID = npc->iNPC_ID;
}

static void eggPickup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SHINY_PICKUP))
        return; // malformed packet

    sP_CL2FE_REQ_SHINY_PICKUP* pickup = (sP_CL2FE_REQ_SHINY_PICKUP*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int eggId = pickup->iShinyID;

    if (Eggs::Eggs.find(eggId) == Eggs::Eggs.end()) {
        std::cout << "[WARN] Player tried to open non existing egg?!" << std::endl;
        return;
    }
    Egg* egg = Eggs::Eggs[eggId];

    if (egg->dead) {
        std::cout << "[WARN] Player tried to open a dead egg?!" << std::endl;
        return;
    }

    /* this has some issues with position desync, leaving it out for now
    if (abs(egg->appearanceData.iX - plr->x)>500 || abs(egg->appearanceData.iY - plr->y) > 500) {
        std::cout << "[WARN] Player tried to open an egg from the other chunk?!" << std::endl;
        return;
    }
    */

    int typeId = egg->appearanceData.iNPCType;
    if (EggTypes.find(typeId) == EggTypes.end()) {
        std::cout << "[WARN] Egg Type " << typeId << " not found!" << std::endl;
        return;
    }

    EggType* type = &EggTypes[typeId];

    // buff the player
    if (type->effectId != 0)
        eggBuffPlayer(sock, type->effectId, eggId, type->duration);

    /*
     * SHINY_PICKUP_SUCC is only causing a GUI effect in the client
     * (buff icon pops up in the bottom of the screen)
     * so we don't send it for non-effect
     */

    if (type->effectId != 0)
    {
        INITSTRUCT(sP_FE2CL_REP_SHINY_PICKUP_SUCC, resp);
        resp.iSkillID = type->effectId;

        // in general client finds correct icon on it's own,
        // but for damage we have to supply correct CSTB
        if (resp.iSkillID == 183)
            resp.eCSTB = ECSB_INFECTION;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_SHINY_PICKUP_SUCC, sizeof(sP_FE2CL_REP_SHINY_PICKUP_SUCC));
    }

    // drop
    if (type->dropCrateId != 0) {
        const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        // we know it's only one trailing struct, so we can skip full validation

        uint8_t respbuf[resplen]; // not a variable length array, don't worry
        sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
        sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

        // don't forget to zero the buffer!
        memset(respbuf, 0, resplen);

        // send back player's stats
        reward->m_iCandy = plr->money;
        reward->m_iFusionMatter = plr->fusionmatter;
        reward->m_iBatteryN = plr->batteryN;
        reward->m_iBatteryW = plr->batteryW;
        reward->iFatigue = 100; // prevents warning message
        reward->iFatigue_Level = 1;
        reward->iItemCnt = 1; // remember to update resplen if you change this

        int slot = Items::findFreeSlot(plr);

        // no space for drop
        if (slot != -1) {

            // item reward
            item->sItem.iType = 9;
            item->sItem.iOpt = 1;
            item->sItem.iID = type->dropCrateId;
            item->iSlotNum = slot;
            item->eIL = 1; // Inventory Location. 1 means player inventory.

            // update player
            plr->Inven[slot] = item->sItem;
            sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
        }
    }

    if (egg->summoned)
        NPCManager::destroyNPC(eggId);
    else {
        Chunking::removeNPCFromChunks(Chunking::getViewableChunks(egg->chunkPos), eggId);
        egg->dead = true;
        egg->deadUntil = getTime() + (time_t)type->regen * 1000;
        egg->appearanceData.iHP = 0;
    }
}

void Eggs::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SHINY_PICKUP, eggPickup);

    REGISTER_SHARD_TIMER(eggStep, 1000);
}
