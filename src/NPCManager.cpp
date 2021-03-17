#include "OpenFusion.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>
#include <assert.h>
#include <limits.h>


using namespace NPCManager;

std::map<int32_t, BaseNPC*> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;
nlohmann::json NPCManager::NPCData;

/*
 * Initialized at the end of TableData::init().
 * This allows us to summon and kill mobs in arbitrary order without
 * NPC ID collisions.
 */
int32_t NPCManager::nextId;

void NPCManager::destroyNPC(int32_t id) {
    // sanity check
    if (NPCs.find(id) == NPCs.end()) {
        std::cout << "npc not found: " << id << std::endl;
        return;
    }

    BaseNPC* entity = NPCs[id];

    // sanity check
    if (!Chunking::chunkExists(entity->chunkPos)) {
        std::cout << "chunk not found!" << std::endl;
        return;
    }

    // remove NPC from the chunk
    Chunking::untrackNPC(entity->chunkPos, id);

    // remove from viewable chunks
    Chunking::removeNPCFromChunks(Chunking::getViewableChunks(entity->chunkPos), id);

    // remove from mob manager
    if (MobAI::Mobs.find(id) != MobAI::Mobs.end())
        MobAI::Mobs.erase(id);

    // remove from eggs
    if (Eggs::Eggs.find(id) != Eggs::Eggs.end())
        Eggs::Eggs.erase(id);

    // finally, remove it from the map and free it
    delete entity->viewableChunks;
    NPCs.erase(id);
    delete entity;
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z, uint64_t I, int angle) {
    BaseNPC* npc = NPCs[id];
    npc->appearanceData.iAngle = angle;
    ChunkPos oldChunk = npc->chunkPos;
    ChunkPos newChunk = Chunking::chunkPosAt(X, Y, I);
    npc->appearanceData.iX = X;
    npc->appearanceData.iY = Y;
    npc->appearanceData.iZ = Z;
    npc->instanceID = I;
    if (oldChunk == newChunk)
        return; // didn't change chunks
    Chunking::updateNPCChunk(id, oldChunk, newChunk);
}

void NPCManager::sendToViewable(BaseNPC *npc, void *buf, uint32_t type, size_t size) {
    for (auto it = npc->viewableChunks->begin(); it != npc->viewableChunks->end(); it++) {
        Chunk* chunk = *it;
        for (CNSocket *s : chunk->players) {
            s->sendPacket(buf, type, size);
        }
    }
}

static void npcBarkHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_BARKER))
        return; // malformed packet

    sP_CL2FE_REQ_BARKER* req = (sP_CL2FE_REQ_BARKER*)data->buf;

    // get bark IDs from task data
    TaskData* td = Missions::Tasks[req->iMissionTaskID];
    std::vector<int> barks;
    for (int i = 0; i < 4; i++) {
        if (td->task["m_iHBarkerTextID"][i] != 0) // non-zeroes only
            barks.push_back(td->task["m_iHBarkerTextID"][i]);
    }

    if (barks.empty())
        return; // no barks

    INITSTRUCT(sP_FE2CL_REP_BARKER, resp);
    resp.iNPC_ID = req->iNPC_ID;
    resp.iMissionStringID = barks[rand() % barks.size()];
    sock->sendPacket((void*)&resp, P_FE2CL_REP_BARKER, sizeof(sP_FE2CL_REP_BARKER));
}

static void npcUnsummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_UNSUMMON))
        return; // malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::destroyNPC(req->iNPC_ID);
}

// type must already be checked and updateNPCPosition() must be called on the result
BaseNPC *NPCManager::summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn, bool baseInstance) {
    uint64_t inst = baseInstance ? MAPNUM(instance) : instance;
#define EXTRA_HEIGHT 0

    assert(nextId < INT32_MAX);
    int id = nextId++;
    int team = NPCData[type]["m_iTeam"];
    BaseNPC *npc = nullptr;

    if (team == 2) {
        npc = new Mob(x, y, z + EXTRA_HEIGHT, inst, type, NPCData[type], id);
        MobAI::Mobs[id] = (Mob*)npc;

        // re-enable respawning, if desired
        ((Mob*)npc)->summoned = !respawn;
    } else
        npc = new BaseNPC(x, y, z + EXTRA_HEIGHT, 0, inst, type, id);

    NPCs[id] = npc;

    return npc;
}

static void npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int limit = NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (plr->accountLevel > 30 || req->iNPCType > limit || req->iNPCCnt > 100)
        return;

    for (int i = 0; i < req->iNPCCnt; i++) {
        BaseNPC *npc = summonNPC(plr->x, plr->y, plr->z, plr->instanceID, req->iNPCType);
        updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, plr->instanceID, 0);
    }
}

static void handleWarp(CNSocket* sock, int32_t warpId) {
    Player* plr = PlayerManager::getPlayer(sock);
    // sanity check
    if (Warps.find(warpId) == Warps.end())
        return;

    if (plr->iPCState & 8) {
        // remove the player's vehicle
        plr->iPCState &= ~8;

        // send to self
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, off);
        sock->sendPacket((void*)&off, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));

        // send to others
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
        chg.iPC_ID = plr->iID;
        chg.iState = plr->iPCState;
        PlayerManager::sendToViewable(sock, (void*)&chg, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
    }

    // std::cerr << "Warped to Map Num:" << Warps[warpId].instanceID << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance) {
        uint64_t instanceID = Warps[warpId].instanceID;

        // if warp requires you to be on a mission, it's gotta be a unique instance
        if (Warps[warpId].limitTaskID != 0 || instanceID == 14) { // 14 is a special case for the Time Lab
            instanceID += ((uint64_t)plr->iIDGroup << 32); // upper 32 bits are leader ID
            Chunking::createInstance(instanceID);

            // save Lair entrance coords as a pseudo-Resurrect 'Em
            plr->recallX = Warps[warpId].x;
            plr->recallY = Warps[warpId].y;
            plr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
            plr->recallInstance = instanceID;
        }

        if (plr->iID == plr->iIDGroup && plr->groupCnt == 1)
            PlayerManager::sendPlayerTo(sock, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
        else {
            Player* leaderPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            for (int i = 0; i < leaderPlr->groupCnt; i++) {
                Player* otherPlr = PlayerManager::getPlayerFromID(leaderPlr->groupIDs[i]);
                CNSocket* sockTo = PlayerManager::getSockFromID(leaderPlr->groupIDs[i]);

                if (otherPlr == nullptr || sockTo == nullptr)
                    continue;

                // save Lair entrance coords for everyone else as well
                otherPlr->recallX = Warps[warpId].x;
                otherPlr->recallY = Warps[warpId].y;
                otherPlr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
                otherPlr->recallInstance = instanceID;

                // remove their vehicle if they're on one
                if (otherPlr->iPCState & 8) {
                    // remove the player's vehicle
                    otherPlr->iPCState &= ~8;

                    // send to self
                    INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, off);
                    sockTo->sendPacket((void*)&off, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));

                    // send to others
                    INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
                    chg.iPC_ID = otherPlr->iID;
                    chg.iState = otherPlr->iPCState;
                    PlayerManager::sendToViewable(sockTo, (void*)&chg, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
                }

                PlayerManager::sendPlayerTo(sockTo, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
            }
        }
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); // Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpId].x;
        resp.iY = Warps[warpId].y;
        resp.iZ = Warps[warpId].z;
        resp.iCandy = plr->money;
        resp.eIL = 4; // do not take away any items
        uint64_t fromInstance = plr->instanceID; // pre-warp instance, saved for post-warp
        plr->instanceID = INSTANCE_OVERWORLD;
        Missions::failInstancedMissions(sock); // fail any instanced missions
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));

        Chunking::updatePlayerChunk(sock, plr->chunkPos, std::make_tuple(0, 0, 0)); // force player to reload chunks
        PlayerManager::updatePlayerPosition(sock, resp.iX, resp.iY, resp.iZ, INSTANCE_OVERWORLD, plr->angle);

        // remove the player's ongoing race, if any
        if (Racing::EPRaces.find(sock) != Racing::EPRaces.end())
            Racing::EPRaces.erase(sock);

        // post-warp: check if the source instance has no more players in it and delete it if so
        Chunking::destroyInstanceIfEmpty(fromInstance);
    }
}

static void npcWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_NPC* warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;
    handleWarp(sock, warpNpc->iWarpID);
}

static void npcWarpTimeMachine(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TIME_TO_GO_WARP))
        return; // malformed packet
    // this is just a warp request
    handleWarp(sock, 28);
}

/*
 * Helper function to get NPC closest to coordinates in specified chunks
 */
BaseNPC* NPCManager::getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z) {
    BaseNPC* npc = nullptr;
    int lastDist = INT_MAX;
    for (auto c = chunks->begin(); c != chunks->end(); c++) { // haha get it
        Chunk* chunk = *c;
        for (auto _npc = chunk->NPCs.begin(); _npc != chunk->NPCs.end(); _npc++) {
            BaseNPC* npcTemp = NPCs[*_npc];
            int distXY = std::hypot(X - npcTemp->appearanceData.iX, Y - npcTemp->appearanceData.iY);
            int dist = std::hypot(distXY, Z - npcTemp->appearanceData.iZ);
            if (dist < lastDist) {
                npc = npcTemp;
                lastDist = dist;
            }
        }
    }
    return npc;
}

// TODO: Move this to MobAI, possibly
#pragma region NPCEvents

// summon right arm and stage 2 body
static void lordFuseStageTwo(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc; // adaptium, stun
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage two" << std::endl;

    // Fuse doesn't move; spawnX, etc. is shorter to write than *appearanceData*
    // Blastons, Heal
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2467);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // right arm, Adaptium, Stun
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX - 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2469);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

// summon left arm and stage 3 body
static void lordFuseStageThree(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc;
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage three" << std::endl;

    // Cosmix, Damage Point
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2468);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // Blastons, Heal
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX + 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2470);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

std::vector<NPCEvent> NPCManager::NPCEvents = {
    NPCEvent(2466, ON_KILLED, lordFuseStageTwo),
    NPCEvent(2467, ON_KILLED, lordFuseStageThree),
};

#pragma endregion NPCEvents

void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TIME_TO_GO_WARP, npcWarpTimeMachine);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_UNSUMMON, npcUnsummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);
}
