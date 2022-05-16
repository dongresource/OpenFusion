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

void Abilities::init() {
	//REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
