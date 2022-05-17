#include "Abilities.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"
#include "NPCManager.hpp"
#include "Nanos.hpp"
#include "Groups.hpp"
#include "Eggs.hpp"

std::map<int32_t, SkillData> Abilities::SkillTable;

/*
// New email notification
static void emailUpdateCheck(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_REP_PC_NEW_EMAIL, resp);
    resp.iNewEmailCnt = Database::getUnreadEmailCount(PlayerManager::getPlayer(sock)->iID);
    sock->sendPacket(resp, P_FE2CL_REP_PC_NEW_EMAIL);
}
*/

std::vector<EntityRef> Abilities::matchTargets(SkillData *skill, int count, int32_t *ids) {
    
    std::vector<int> tempTargs;
    switch (skill->effectTarget)
    {
    case SkillEffectTarget::POINT:
        std::cout << "[SKILL] POINT; ";
        break;
    case SkillEffectTarget::SELF:
        std::cout << "[SKILL] SELF; ";
        break;
    case SkillEffectTarget::CONE:
        std::cout << "[SKILL] CONE; ";
        break;
    case SkillEffectTarget::AREA_SELF:
        std::cout << "[SKILL] AREA_SELF; ";
        break;
    case SkillEffectTarget::AREA_TARGET:
        std::cout << "[SKILL] AREA_TARGET; ";
        break;
    }

    for (int i = 0; i < count; i++) std::cout << ids[i] << " ";
    std::cout << std::endl;

    std::vector<EntityRef> targets;
    return targets;
}

void Abilities::init() {
    //REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
