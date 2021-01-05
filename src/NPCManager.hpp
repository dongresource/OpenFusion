#pragma once

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include "contrib/JSON.hpp"

#include <map>
#include <unordered_map>
#include <vector>

#define RESURRECT_HEIGHT 400

// placeholder; there's only one trigger type right now
enum Trigger {
    ON_KILLED
};

typedef void (*NPCEventHandler)(CNSocket*, BaseNPC*);

struct NPCEvent {
    int32_t npcType;
    int trigger;
    NPCEventHandler handler;

    NPCEvent(int32_t t, int tr, NPCEventHandler hndlr)
        : npcType(t), trigger(tr), handler(hndlr) {}
};

// this should really be called vec3 or something...
struct WarpLocation {
    int x, y, z, instanceID, isInstance, limitTaskID, npcID;
};

struct Egg : public BaseNPC {
    bool summoned;
    bool dead = false;
    time_t deadUntil;

    Egg(int x, int y, int z, uint64_t iID, int type, int32_t id, bool summon)
        : BaseNPC(x, y, z, 0, iID, type, id) {
        summoned = summon;
        npcClass = NPCClass::NPC_EGG;
    }
};

struct EggType {
    int dropCrateId;
    int effectId;
    int duration;
    int regen;
};

namespace NPCManager {
    extern std::map<int32_t, BaseNPC*> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;
    extern std::vector<WarpLocation> RespawnPoints;
    extern std::vector<NPCEvent> NPCEvents;
    extern std::unordered_map<int, Egg*> Eggs;
    extern std::map<std::pair<CNSocket*, int32_t>, time_t> EggBuffs;
    extern std::unordered_map<int, EggType> EggTypes;
    extern nlohmann::json NPCData;
    extern int32_t nextId;
    void init();

    void destroyNPC(int32_t);
    void updateNPCPosition(int32_t, int X, int Y, int Z, uint64_t I, int angle);

    void sendToViewable(BaseNPC* npc, void* buf, uint32_t type, size_t size);

    void npcBarkHandler(CNSocket* sock, CNPacketData* data);
    void npcSummonHandler(CNSocket* sock, CNPacketData* data);
    void npcUnsummonHandler(CNSocket* sock, CNPacketData* data);
    void npcWarpHandler(CNSocket* sock, CNPacketData* data);
    void npcWarpTimeMachine(CNSocket* sock, CNPacketData* data);

    void npcVendorStart(CNSocket* sock, CNPacketData* data);
    void npcVendorTable(CNSocket* sock, CNPacketData* data);
    void npcVendorBuy(CNSocket* sock, CNPacketData* data);
    void npcVendorSell(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyback(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyBattery(CNSocket* sock, CNPacketData* data);

    void handleWarp(CNSocket* sock, int32_t warpId);
    BaseNPC *summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn=false, bool baseInstance=false);

    BaseNPC* getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z);

    /// returns -1 on fail
    int eggBuffPlayer(CNSocket* sock, int skillId, int duration);
    void eggStep(CNServer* serv, time_t currTime);
    void npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg);
    void eggPickup(CNSocket* sock, CNPacketData* data);
}
