#pragma once

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"

#include <algorithm>
#include <cstring>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "JSON.hpp"

class Chunk {
public:
    std::set<CNSocket*> players;
    std::set<int32_t> NPCs;
};

#define ACTIVE_MISSION_COUNT 6

#define PC_MAXHEALTH(level) (925 + 75 * (level))

struct Player {
    int accountId;
    int accountLevel; // permission level (see CN_ACCOUNT_LEVEL enums)
    int64_t SerialKey;
    int32_t iID;
    uint64_t FEKey;

    int level;
    int HP;
    int slot; // player slot, not nano slot
    int16_t mentor;
    int32_t money;
    int32_t fusionmatter;
    int32_t batteryW;
    int32_t batteryN;
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;
    sNano Nanos[NANO_COUNT]; // acquired nanos
    int equippedNanos[3];
    int activeNano; // active nano (index into Nanos)
    int8_t iPCState;
    int32_t iWarpLocationFlag;
    int64_t aSkywayLocationFlag[2];
    int32_t iConditionBitFlag;
    int32_t iSelfConditionBitFlag;
    int8_t iSpecialState;

    int x, y, z, angle;
    int lastX, lastY, lastZ, lastAngle;
    int recallX, recallY, recallZ, recallInstance; // also Lair entrances
    uint64_t instanceID;
    sItemBase Equip[AEQUIP_COUNT];
    sItemBase Inven[AINVEN_COUNT];
    sItemBase Bank[ABANK_COUNT];
    sItemTrade Trade[12];
    int32_t moneyInTrade;
    bool isTrading;
    bool isTradeConfirm;

    bool inCombat;
    bool onMonkey;
    int nanoDrainRate;
    int healCooldown;

    int pointDamage;
    int groupDamage;
    int fireRate;
    int defense;

    int64_t aQuestFlag[16];
    int tasks[ACTIVE_MISSION_COUNT];
    int RemainingNPCCount[ACTIVE_MISSION_COUNT][3];
    sItemBase QInven[AQINVEN_COUNT];
    int32_t CurrentMissionID;

    sTimeLimitItemDeleteInfo2CL toRemoveVehicle;

    int32_t iIDGroup;
    int groupCnt;
    int32_t groupIDs[4];
    int32_t iGroupConditionBitFlag;

    bool notify;
    bool hidden;
    bool unwarpable;

    bool buddiesSynced;
    int64_t buddyIDs[50];
    bool isBuddyBlocked[50];

    uint64_t iFirstUseFlag[2];

    ChunkPos chunkPos;
    std::set<Chunk*> *viewableChunks;
    time_t lastHeartbeat;

    int suspicionRating;
    time_t lastShot;
    std::vector<sItemBase> *buyback;
};


class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;
    uint64_t instanceID;
    ChunkPos chunkPos;
    std::set<Chunk*>* viewableChunks;

    int playersInView;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = angle;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        npcClass = NPCClass::NPC_BASE;

        instanceID = iID;

        chunkPos = std::make_tuple(0, 0, 0);
        viewableChunks = new std::set<Chunk*>();
        playersInView = 0;
    };
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id, NPCClass classType) : BaseNPC(x, y, z, angle, iID, type, id) {
        npcClass = classType;
    }
};


enum class MobState {
    INACTIVE,
    ROAMING,
    COMBAT,
    RETREAT,
    DEAD
};

struct Mob : public BaseNPC {
    // general
    MobState state;
    int maxHealth;
    int spawnX;
    int spawnY;
    int spawnZ;
    int level;

    std::unordered_map<int32_t,time_t> unbuffTimes;

    // dead
    time_t killedTime = 0;
    time_t regenTime;
    bool summoned = false;
    bool despawned = false; // for the sake of death animations

    // roaming
    int idleRange;
    const int sightRange;
    time_t nextMovement = 0;
    bool staticPath = false;
    int roamX, roamY, roamZ;

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;
    time_t lastDrainTime = 0;
    int skillStyle = -1; // -1 for nothing, 0-2 for corruption, -2 for eruption
    int hitX, hitY, hitZ; // for use in ability targeting

    // drop
    int dropType;

    // group
    int groupLeader = 0;
    int offsetX, offsetY;
    int groupMember[4] = {0, 0, 0, 0};

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int angle, uint64_t iID, int type, nlohmann::json d, int32_t id)
        : BaseNPC(x, y, z, angle, iID, type, id),
          maxHealth(d["m_iHP"]),
          sightRange(d["m_iSightRange"]) {
        state = MobState::ROAMING;

        data = d;

        regenTime = data["m_iRegenTime"];
        idleRange = (int)data["m_iIdleRange"];
        dropType = data["m_iDropType"];
        level = data["m_iNpcLevel"];

        roamX = spawnX = appearanceData.iX;
        roamY = spawnY = appearanceData.iY;
        roamZ = spawnZ = appearanceData.iZ;

        offsetX = 0;
        offsetY = 0;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;

        npcClass = NPC_MOB;
    }

    // constructor for /summon
    Mob(int x, int y, int z, uint64_t iID, int type, nlohmann::json d, int32_t id)
        : Mob(x, y, z, 0, iID, type, d, id) {
        summoned = true; // will be despawned and deallocated when killed
    }

    ~Mob() {}

    auto operator[](std::string s) {
        return data[s];
    }
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

typedef void (*PowerHandler)(CNSocket*, std::vector<int>, int16_t, int16_t, int16_t, int16_t, int16_t, int32_t, int16_t);

struct NanoPower {
    int16_t skillType;
    int32_t bitFlag;
    int16_t timeBuffID;
    PowerHandler handler;

    NanoPower(int16_t s, int32_t b, int16_t t, PowerHandler h) : skillType(s), bitFlag(b), timeBuffID(t), handler(h) {}

    void handle(CNSocket *sock, std::vector<int> targetData, int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount) {
        if (handler == nullptr)
            return;

        handler(sock, targetData, nanoID, skillID, duration, amount, skillType, bitFlag, timeBuffID);
    }
};

typedef void (*MobPowerHandler)(Mob*, std::vector<int>, int16_t, int16_t, int16_t, int16_t, int32_t, int16_t);

struct MobPower {
    int16_t skillType;
    int32_t bitFlag;
    int16_t timeBuffID;
    MobPowerHandler handler;

    MobPower(int16_t s, int32_t b, int16_t t, MobPowerHandler h) : skillType(s), bitFlag(b), timeBuffID(t), handler(h) {}

    void handle(Mob *mob, std::vector<int> targetData, int16_t skillID, int16_t duration, int16_t amount) {
        if (handler == nullptr)
            return;

        handler(mob, targetData, skillID, duration, amount, skillType, bitFlag, timeBuffID);
    }
};

struct SkillData {
    int skillType;
    int targetType;
    int drainType;
    int effectArea;
    int batteryUse[4];
    int durationTime[4];
    int powerIntensity[4];
};

namespace Nanos {
    extern std::vector<NanoPower> NanoPowers;
    extern std::map<int32_t, SkillData> SkillTable;

    void nanoUnbuff(CNSocket* sock, std::vector<int> targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower);
    int applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags);

    std::vector<int> findTargets(Player* plr, int skillID, CNPacketData* data = nullptr);
}

namespace Combat {
    extern std::vector<MobPower> MobPowers;
}


namespace Buddies {
	void init();

	// Buddy list
	void refreshBuddyList(CNSocket* sock);
}


namespace BuiltinCommands {
    void init();

    void setSpecialState(CNSocket *sock, CNPacketData *data);
};

#define CMD_PREFIX '/'


namespace Chat {
    extern std::vector<std::string> dump;
    void init();

    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD
    std::string sanitizeText(std::string text, bool allowNewlines=false);
}



enum {
    INSTANCE_OVERWORLD, // default instance every player starts in
    INSTANCE_IZ, // these aren't actually used
    INSTANCE_UNIQUE // these aren't actually used
};

namespace Chunking {
    extern std::map<ChunkPos, Chunk*> chunks;

    void updatePlayerChunk(CNSocket* sock, ChunkPos from, ChunkPos to);
    void updateNPCChunk(int32_t id, ChunkPos from, ChunkPos to);

    void trackPlayer(ChunkPos chunkPos, CNSocket* sock);
    void trackNPC(ChunkPos chunkPos, int32_t id);
    void untrackPlayer(ChunkPos chunkPos, CNSocket* sock);
    void untrackNPC(ChunkPos chunkPos, int32_t id);

    void addPlayerToChunks(std::set<Chunk*> chnks, CNSocket* sock);
    void addNPCToChunks(std::set<Chunk*> chnks, int32_t id);
    void removePlayerFromChunks(std::set<Chunk*> chnks, CNSocket* sock);
    void removeNPCFromChunks(std::set<Chunk*> chnks, int32_t id);

    bool chunkExists(ChunkPos chunk);
    ChunkPos chunkPosAt(int posX, int posY, uint64_t instanceID);
    std::set<Chunk*> getViewableChunks(ChunkPos chunkPos);

    bool inPopulatedChunks(std::set<Chunk*>* chnks);
    void createInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}




struct Bullet {
    int pointDamage;
    int groupDamage;
    bool weaponBoost;
    int bulletType;
};

namespace Combat {
    extern std::map<int32_t, std::map<int8_t, Bullet>> Bullets;

    void init();

    void npcAttackPc(Mob *mob, time_t currTime);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);
}


namespace CustomCommands {
    void init();

    bool runCmd(std::string full, CNSocket* sock);
};


struct EggType {
    int dropCrateId;
    int effectId;
    int duration;
    int regen;
};

namespace Eggs {
    extern std::unordered_map<int, Egg*> Eggs;
    extern std::map<std::pair<CNSocket*, int32_t>, time_t> EggBuffs;
    extern std::unordered_map<int, EggType> EggTypes;

    void init();

    /// returns -1 on fail
    int eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration);
    void npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg);
}

namespace Email {
	void init();
}



namespace Groups {
	void init();

    void sendToGroup(Player* plr, void* buf, uint32_t type, size_t size);
    void groupTickInfo(Player* plr);
    void groupKickPlayer(Player* plr);
    int getGroupFlags(Player* plr);
}


struct CrocPotEntry {
    int multStats, multLooks;
    float base, rd0, rd1, rd2, rd3;
};

struct Crate {
    int rarityRatioId;
    std::vector<int> itemSets;
};

struct MobDropChance {
    int dropChance;
    std::vector<int> cratesRatio;
};

struct MobDrop {
    std::vector<int> crateIDs;
    int dropChanceType;
    int taros;
    int fm;
    int boosts;
};

namespace Items {
    enum class SlotType {
        EQUIP = 0,
        INVENTORY = 1,
        BANK = 3
    };
    struct Item {
        bool tradeable, sellable;
        int buyPrice, sellPrice;
        int stackSize, level, rarity;
        int pointDamage, groupDamage, fireRate, defense, gender;
        int weaponType;
        // TODO: implement more as needed
    };
    // hopefully this is fine since it's never modified after load
    extern std::map<std::pair<int32_t, int32_t>, Item> ItemData; // <id, type> -> data
    extern std::map<int32_t, CrocPotEntry> CrocPotTable; // level gap -> entry
    extern std::map<int32_t, std::vector<int>> RarityRatios;
    extern std::map<int32_t, Crate> Crates;
    // pair <Itemset, Rarity> -> vector of pointers (map iterators) to records in ItemData (it looks a lot scarier than it is)
    extern std::map<std::pair<int32_t, int32_t>,
        std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator>> CrateItems;
    extern std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> CodeItems; // code -> vector of <id, type>

    // mob drops
    extern std::map<int32_t, MobDropChance> MobDropChances;
    extern std::map<int32_t, MobDrop> MobDrops;

    void init();

    // mob drops
    void giveMobDrop(CNSocket *sock, Mob *mob, int rolledBoosts, int rolledPotions, int rolledCrate, int rolledCrateType, int rolledEvent);

    int findFreeSlot(Player *plr);
    Item* getItemData(int32_t id, int32_t type);
    void checkItemExpire(CNSocket* sock, Player* player);
    void setItemStats(Player* plr);
    void updateEquips(CNSocket* sock, Player* plr);

#ifdef ACADEMY
    extern std::map<int32_t, int32_t> NanoCapsules; // crate id -> nano id
#endif
}



struct Reward {
    int32_t id;
    int32_t itemTypes[4];
    int32_t itemIds[4];
    int32_t money;
    int32_t fusionmatter;

    Reward(int32_t id, nlohmann::json types, nlohmann::json ids, int32_t m, int32_t fm) :
    id(id), money(m), fusionmatter(fm) {
        for (int i = 0; i < 4; i++) {
            itemTypes[i] = types[i];
            itemIds[i] = ids[i];
        }
    };
};

struct TaskData {
    /*
     * TODO: We'll probably want to keep only the data the server actually needs,
     *       but for now RE/development is much easier if we have everything at
     *       our fingertips.
     */
    nlohmann::json task;

    TaskData(nlohmann::json t) : task(t) {}

    // convenience
    auto operator[](std::string s) { return task[s]; }
};

namespace Missions {
    extern std::map<int32_t, Reward*> Rewards;
    extern std::map<int32_t, TaskData*> Tasks;
    extern nlohmann::json AvatarGrowth[37];
    void init();

    bool startTask(Player* plr, int TaskID);

    // checks if player doesn't have n/n quest items
    void updateFusionMatter(CNSocket* sock, int fusion);

    void mobKilled(CNSocket *sock, int mobid, int rolledQItem);

    void quitTask(CNSocket* sock, int32_t taskNum, bool manual);

    void failInstancedMissions(CNSocket* sock);
}


namespace MobAI {
    extern bool simulateMobs;
    extern std::map<int32_t, Mob*> Mobs;

    void init();

    // TODO: make this internal later
    void incNextMovement(Mob *mob, time_t currTime=0);
    bool aggroCheck(Mob *mob, time_t currTime);
    void clearDebuff(Mob *mob);
    void followToCombat(Mob *mob);
    void groupRetreat(Mob *mob);
    void enterCombat(CNSocket *sock, Mob *mob);
}



struct NanoData {
    int style;
};

struct NanoTuning {
    int reqItemCount;
    int reqItems;
};

namespace Nanos {
    extern std::map<int32_t, NanoData> NanoTable;
    extern std::map<int32_t, NanoTuning> NanoTunings;
    void init();

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm=false);
    void summonNano(CNSocket* sock, int slot, bool silent = false);
    int nanoStyle(int nanoID);
    bool getNanoBoost(Player* plr);
}


#define RESURRECT_HEIGHT 400

enum Trigger {
    ON_KILLED,
    ON_COMBAT
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

namespace NPCManager {
    extern std::map<int32_t, BaseNPC*> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;
    extern std::vector<WarpLocation> RespawnPoints;
    extern std::vector<NPCEvent> NPCEvents;
    extern nlohmann::json NPCData;
    extern int32_t nextId;
    void init();

    void destroyNPC(int32_t);
    void updateNPCPosition(int32_t, int X, int Y, int Z, uint64_t I, int angle);

    void sendToViewable(BaseNPC* npc, void* buf, uint32_t type, size_t size);

    BaseNPC *summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn=false, bool baseInstance=false);

    BaseNPC* getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z);
}



struct WarpLocation;

namespace PlayerManager {
    extern std::map<CNSocket*, Player*> players;
    void init();

    void removePlayer(CNSocket* key);

    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, uint64_t I, int angle);

    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I);
    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z);

    void sendToViewable(CNSocket* sock, void* buf, uint32_t type, size_t size);

    Player *getPlayer(CNSocket* key);
    std::string getPlayerName(Player *plr, bool id=true);

    bool isAccountInUse(int accountId);
    void exitDuplicate(int accountId);
    Player *getPlayerFromID(int32_t iID);
    CNSocket *getSockFromID(int32_t iID);
    CNSocket *getSockFromName(std::string firstname, std::string lastname);
    CNSocket *getSockFromAny(int by, int id, int uid, std::string firstname, std::string lastname);
}

namespace PlayerMovement {
    void init();
};


struct EPInfo {
    int zoneX, zoneY, EPID, maxScore, maxTime;
};

struct EPRace {
    int ringCount, mode, ticketSlot;
    time_t startTime;
};

namespace Racing {
    extern std::map<int32_t, EPInfo> EPData;
    extern std::map<CNSocket*, EPRace> EPRaces;
    extern std::map<int32_t, std::pair<std::vector<int>, std::vector<int>>> EPRewards;

    void init();
}

namespace TableData {
    extern std::map<int32_t, std::vector<WarpLocation>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, int> RunningNPCMapNumbers;
    extern std::map<int32_t, BaseNPC*> RunningMobs;
    extern std::map<int32_t, BaseNPC*> RunningGroups;
    extern std::map<int32_t, BaseNPC*> RunningEggs;

    void init();
    void flush();
}


namespace Trading {
    void init();
}


const int SLIDER_SPEED = 1200;
const int SLIDER_STOP_TICKS = 16;
const int SLIDER_GAP_SIZE = 45000;

struct WarpLocation;

struct TransportRoute {
    int type, start, end, cost, mssSpeed, mssRouteNum;
};

struct TransportLocation {
    int npcID, x, y, z;
};

namespace Transport {
    extern std::map<int32_t, TransportRoute> Routes;
    extern std::map<int32_t, TransportLocation> Locations;
    extern std::map<int32_t, std::queue<WarpLocation>> SkywayPaths; // predefined skyway paths with points
    extern std::unordered_map<CNSocket*, std::queue<WarpLocation>> SkywayQueues; // player sockets with queued broomstick points
    extern std::unordered_map<int32_t, std::queue<WarpLocation>> NPCQueues; // NPC ids with queued pathing points

    void init();

    void testMssRoute(CNSocket *sock, std::vector<WarpLocation>* route);

    void lerp(std::queue<WarpLocation>*, WarpLocation, WarpLocation, int, float);
    void lerp(std::queue<WarpLocation>*, WarpLocation, WarpLocation, int);
}



struct VendorListing {
    int sort, type, iID;
};

namespace Vendor {
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;

    void init();
}
