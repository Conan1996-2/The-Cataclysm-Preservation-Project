/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Stats.h"
#include "Object.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "Unit.h"

Stats::~Stats() = default;

Stats::Stats(Unit* owner) : _owner(owner), _baseStats({ }), _baseStatModifiers({ }), _basePctMultipliers({ }),
    _totalModifiers({ }), _totalPctMultipliers({ })
{
    _basePctMultipliers.fill(1.0);
    _totalPctMultipliers.fill(1.0);
}

/// Returns the unmodified base value of the specified stat.
/// This is the raw stat value that depends on the unit level.
/// @param statType The stat which base value will be returned
/// @return returns the base value fo the specified stat
int32 Stats::GetBaseStatValue(StatType statType) const
{
    return _baseStats[AsUnderlyingType(statType)];
}

/// Sets the unmodified base value of the specified stat and
/// updates all bonus calculations afterward. This method is
/// meant to be used for initializing level based raw stats
/// @param statType The stat which base value will be changed
/// @param value The new base value that will be set
void Stats::SetBaseStatValue(StatType statType, int32 value)
{
    _baseStats[AsUnderlyingType(statType)] = value;
    updateStat(statType);
}

/// Adds a modifier on specified stat value. This method is meant
/// to be used for applying item and enchantment based bonus effects
/// @param statType The stat which modifier value will be changed
/// @param value The amount of the modifier that will be applied
void Stats::AddBaseStatModifier(StatType statType, int32 value)
{
    _baseStatModifiers[AsUnderlyingType(statType)] += value;
    updateStat(statType);
}

/// Removes a modifier on specified stat value. This method is meant
/// to be used for removing item and enchantment based bonus effects
/// @param statType The stat which modifier value will be changed
/// @param value The amount of the modifier that will be removed
void Stats::RemoveBaseStatModifier(StatType statType, int32 value)
{
    _baseStatModifiers[AsUnderlyingType(statType)] -= value;
    updateStat(statType);
}

/// Updates the stored base percentage multipliers applied by SPELL_AURA_MOD_PERCENT_STAT.
/// This method also applies stacking rules
/// @param statType The stat which multiplier value will be updated
void Stats::UpdateBaseStatMultiplier(StatType statType)
{
    if (statType == StatType::AllStats || statType == StatType::AllStats2)
    {
        for (StatType stat : AllStats)
        {
            float multiplier = getAuraMultiplierForStatType(SPELL_AURA_MOD_PERCENT_STAT, stat);
            _basePctMultipliers[AsUnderlyingType(stat)] = multiplier;
            updateStat(stat);
        }
    }
    else
    {
        float multiplier = getAuraMultiplierForStatType(SPELL_AURA_MOD_PERCENT_STAT, statType);
        _basePctMultipliers[AsUnderlyingType(statType)] = multiplier;
        updateStat(statType);
    }
}

/// Updates the stored base modifiers applied by SPELL_AURA_MOD_STAT.
/// This method also applies stacking rules
/// @param statType The stat which modifier value will be updated
void Stats::UpdateTotalStatModifier(StatType statType)
{
    if (statType == StatType::AllStats || statType == StatType::AllStats2)
    {
        for (StatType stat : AllStats)
        {
            int32 modifier = getAuraModifierForStatType(SPELL_AURA_MOD_STAT, stat);
            _totalModifiers[AsUnderlyingType(stat)] = modifier;
            updateStat(stat);
        }
    }
    else
    {
        int32 modifier = getAuraModifierForStatType(SPELL_AURA_MOD_STAT, statType);
        _totalModifiers[AsUnderlyingType(statType)] = modifier;
        updateStat(statType);
    }
}

/// Updates the stored total percentage multipliers applied by SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE.
/// This method also applies stacking rules
/// @param statType The stat which multiplier value will be updated
void Stats::UpdateTotalStatMultiplier(StatType statType)
{
    if (statType == StatType::AllStats || statType == StatType::AllStats2)
    {
        for (StatType stat : AllStats)
        {
            float multiplier = getAuraMultiplierForStatType(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, stat);
            _totalPctMultipliers[AsUnderlyingType(stat)] = multiplier;
            updateStat(stat);
        }
    }
    else
    {
        float multiplier = getAuraMultiplierForStatType(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, statType);
        _totalPctMultipliers[AsUnderlyingType(statType)] = multiplier;
        updateStat(statType);
    }
}

/// Returns the unit's primary stat type based on its class and specialization
/// @return The primarily used stat of the unit
StatType Stats::GetPrimaryStat() const
{
    switch (_owner->getClass())
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
        {
            if (Player* player = Object::ToPlayer(_owner))
            {
                uint32 talentTree = player->GetPrimaryTalentTree(player->GetActiveSpec());
                if (talentTree == TALENT_TREE_WARRIOR_PROTECTION || talentTree == TALENT_TREE_DEATH_KNIGHT_BLOOD)
                    return StatType::Stamina;
            }
            return StatType::Strength;
        }
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            return StatType::Agility;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
            return StatType::Intellect;
        case CLASS_DRUID:
            if (Player* player = Object::ToPlayer(_owner))
            {
                if (player->GetPrimaryTalentTree(player->GetActiveSpec()) == TALENT_TREE_DRUID_FERAL_COMBAT)
                {
                    if (player->GetShapeshiftForm() == FORM_BEAR)
                        return StatType::Stamina;

                    return StatType::Agility;
                }
            }
            return StatType::Intellect;
        case CLASS_SHAMAN:
            if (Player* player = Object::ToPlayer(_owner))
                if (player->GetPrimaryTalentTree(player->GetActiveSpec()) == TALENT_TREE_SHAMAN_ENHANCEMENT)
                    return StatType::Agility;

            return StatType::Intellect;
        case CLASS_PALADIN:
        {
            if (Player* player = Object::ToPlayer(_owner))
            {
                uint32 talentTree = player->GetPrimaryTalentTree(player->GetActiveSpec());
                if (talentTree == TALENT_TREE_PALADIN_PROTECTION)
                    return StatType::Stamina;

                if (talentTree == TALENT_TREE_PALADIN_HOLY)
                    return StatType::Intellect;
            }
            return StatType::Strength;
        }
        default:
            return StatType::Strength;
    }
}

void Stats::updateStat(StatType statType)
{
    int32 negStatValue = 0;
    int32 posStatValue = 0;

    // ==== Calculate the base flat value (Level stats and item/enchantment mods)
    int32 baseStatValue = _baseStats[AsUnderlyingType(statType)];
    int32 baseModifier = _baseStatModifiers[AsUnderlyingType(statType)];
    if (baseModifier > 0)
        posStatValue += baseModifier;
    else
        negStatValue += baseModifier;

    int32 flatStatValue = baseStatValue + baseModifier;

    // ==== Calculate the total base value (Base pct multiplier auras)
    double basePctMultiplier = _basePctMultipliers[AsUnderlyingType(statType)];
    int32 baseTotalValue = static_cast<int32>(std::round(flatStatValue * basePctMultiplier));
    if (flatStatValue < baseTotalValue)
        posStatValue += baseTotalValue - flatStatValue;
    else
        negStatValue += baseTotalValue - flatStatValue;

    // ==== Calculate the total value (Total value buff auras)
    int32 totalModifier = _totalModifiers[AsUnderlyingType(statType)];
    if (totalModifier > 0)
        posStatValue += totalModifier;
    else
        negStatValue += totalModifier;

    int32 totalValue = baseTotalValue + totalModifier;

    // ==== Calculate the total value (Total pct multipliers)
    double totalPctMultiplier = _totalPctMultipliers[AsUnderlyingType(statType)];
    int32 statValue = static_cast<int32>(std::round(totalValue * totalPctMultiplier));
    if (totalValue < statValue)
        posStatValue += statValue - totalValue;
    else
        negStatValue +=  statValue - totalValue;

    // ==== Apply the stats
    _owner->SetInt32Value(UNIT_FIELD_STAT0 + AsUnderlyingType(statType), statValue);
    _owner->SetInt32Value(UNIT_FIELD_POSSTAT0 + AsUnderlyingType(statType), posStatValue);
    _owner->SetInt32Value(UNIT_FIELD_NEGSTAT0 + AsUnderlyingType(statType), negStatValue);

    switch (statType)
    {
        case StatType::Strength:
            _owner->UpdateAttackPowerAndDamage();
            break;
        case StatType::Agility:
            _owner->UpdateAttackPowerAndDamage(false);
            _owner->UpdateAttackPowerAndDamage(true);
            _owner->UpdateArmor();
            if (Player* player = Object::ToPlayer(_owner))
            {
                player->UpdateAllCritPercentages();
                player->UpdateDodgePercentage();
            }
            break;
        case StatType::Stamina:
            _owner->UpdateMaxHealth();
            break;
        case StatType::Intellect:
            _owner->UpdateMaxPower(POWER_MANA);
            _owner->UpdatePowerRegeneration(POWER_MANA);
            if (Player* player = Object::ToPlayer(_owner))
            {
                player->UpdateAllSpellCritChances();
                player->UpdateSpellDamageAndHealingBonus();
            }
            break;
        case StatType::Spirit:
            _owner->UpdatePowerRegeneration(POWER_MANA);
            break;
        default:
            break;
    }
}

float Stats::getAuraMultiplierForStatType(AuraType auraType, StatType statType) const
{
    float multiplier = _owner->GetTotalAuraMultiplier(auraType, [&](AuraEffect const* aurEff)
    {
        StatType auraStatType = static_cast<StatType>(aurEff->GetMiscValue());
        if (auraStatType == StatType::AllStats || auraStatType == StatType::AllStats2 || auraStatType == statType)
            return true;

        return false;
    });

    return multiplier;
}

int32 Stats::getAuraModifierForStatType(AuraType auraType, StatType statType) const
{
    int32 modifier = _owner->GetTotalAuraModifier(auraType, [&](AuraEffect const* aurEff)
    {
        StatType auraStatType = static_cast<StatType>(aurEff->GetMiscValue());
        if (auraStatType == StatType::AllStats || auraStatType == StatType::AllStats2 || auraStatType == statType)
            return true;

        return false;
    });

    return modifier;
}
