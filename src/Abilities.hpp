#pragma once

#include "core/Core.hpp"
#include "Combat.hpp"

struct SkillData {
    int skillType;
    int effectTarget;
    int effectType;
    int targetType;
    int batteryDrainType;
    int effectArea;

    int batteryUse[4];
    int durationTime[4];

    int valueTypes[3];
    int values[3][4];
};

namespace Abilities {
    extern std::map<int32_t, SkillData> SkillTable;

    void init();
}
