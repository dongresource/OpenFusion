#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
#include "NPC.hpp"

#include "contrib/JSON.hpp"

#include <map>
#include <unordered_map>
#include <queue>

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

struct Bullet {
    int pointDamage;
    int groupDamage;
    bool weaponBoost;
    int bulletType;
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

namespace MobManager {
    extern std::map<int32_t, Mob*> Mobs;
    extern std::queue<int32_t> RemovalQueue;
    extern std::map<int32_t, MobDropChance> MobDropChances;
    extern std::map<int32_t, MobDrop> MobDrops;
    extern std::map<int32_t, std::map<int8_t, Bullet>> Bullets;
    extern bool simulateMobs;
    extern std::vector<MobPower> MobPowers;

    void init();
    void step(CNServer*, time_t);
    void playerTick(CNServer*, time_t);

    void deadStep(Mob*, time_t);
    void combatStep(Mob*, time_t);
    void retreatStep(Mob*, time_t);
    void roamingStep(Mob*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);
    void dealGooDamage(CNSocket *sock, int amount);

    void npcAttackPc(Mob *mob, time_t currTime);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock, Mob *mob, int rolledBoosts, int rolledPotions, int rolledCrate, int rolledCrateType, int rolledEvent);
    void getReward(sItemBase *reward, MobDrop *drop, MobDropChance *chance, int rolled);
    void giveEventReward(CNSocket* sock, Player* player, int rolled);

    std::pair<int,int> lerp(int, int, int, int, int);
    std::pair<int,int> getDamage(int, int, bool, bool, int, int, int);

    void drainMobHP(Mob *mob, int amount);
    void incNextMovement(Mob *mob, time_t currTime=0);
    bool aggroCheck(Mob *mob, time_t currTime);
    void clearDebuff(Mob *mob);

    void grenadeFire(CNSocket* sock, CNPacketData* data);
    void rocketFire(CNSocket* sock, CNPacketData* data);
    void projectileHit(CNSocket* sock, CNPacketData* data);
    /// returns bullet id
    int8_t addBullet(Player* plr, bool isGrenade);

    void followToCombat(Mob *mob);
    void groupRetreat(Mob *mob);
    void useAbilities(Mob *mob, time_t currTime);
    void dealCorruption(Mob *mob, std::vector<int> targetData, int skillID, int style);
}
