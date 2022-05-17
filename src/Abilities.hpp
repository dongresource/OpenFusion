#pragma once

#include "core/Core.hpp"
#include "Combat.hpp"

enum class SkillEffectTarget {
    POINT = 1,
    SELF = 2,
    CONE = 3,
    WEAPON = 4,
    AREA_SELF = 5,
    AREA_TARGET = 6
};

enum class SkillTargetType {
    MOBS = 1,
    PLAYERS = 2,
    SELF = 3 // only used once by client /shrug
};

struct SkillData {
    int skillType;
    SkillEffectTarget effectTarget;
    int effectType; // always 1?
    SkillTargetType targetType;
    int batteryDrainType;
    int effectArea;

    int batteryUse[4];
    int durationTime[4];

    int valueTypes[3];
    int values[3][4];
};

namespace Abilities {
    extern std::map<int32_t, SkillData> SkillTable;

    std::vector<EntityRef> matchTargets(SkillData*, int, int32_t*);
    void init();
}
