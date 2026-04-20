#pragma once

#include <mq/Plugin.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

enum class DamageType : int
{
	Melee,
	NonMelee,
	DoT,
	DamageShield,
	Crit,
	CritHeal,
	Miss,
	MissedMe,
	HitBy,
	HitByNonMelee,
	PetMelee,
	PetNonMelee,
	DirectHeal,
};

struct DamageRecord
{
	DamageType  type          = DamageType::Melee;
	std::string attackVerb;
	std::string targetName;
	int         targetSpawnID = 0;
	int64_t     damage        = 0;
	bool        isMiss        = false;
	std::string spellName;
	std::chrono::steady_clock::time_point timestamp;
	int         sequence      = 0;
};

struct ActiveDoT
{
	int         targetSpawnID = 0;
	std::string targetName;
	std::string spellName;
	std::chrono::steady_clock::time_point castTime;
};

struct TargetDamageData
{
	int         spawnID     = 0;
	std::string name;
	int64_t     totalDamage    = 0;
	int64_t     critDamage    = 0;
	int64_t     dotDamage     = 0;
	int64_t     dsDamage      = 0;
	int64_t     petDamage     = 0;
	int64_t     nonMeleeDamage = 0;
	int         hitCount      = 0;
	int         missCount     = 0;
	std::chrono::steady_clock::time_point firstHit;
	std::chrono::steady_clock::time_point lastHit;

	float GetDPS() const
	{
		float dur = GetDurationSeconds();
		return dur > 0.0f ? static_cast<float>(totalDamage) / dur : 0.0f;
	}

	float GetAvgDamage() const
	{
		return hitCount > 0 ? static_cast<float>(totalDamage) / static_cast<float>(hitCount) : 0.0f;
	}

	float GetDurationSeconds() const
	{
		if (hitCount == 0)
			return 0.0f;
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(lastHit - firstHit);
		return std::max(1.0f, elapsed.count() / 1000.0f);
	}
};

struct HealTargetData
{
	std::string name;
	int64_t     directHeals = 0;
	int64_t     critHeals   = 0;
	int         healCount   = 0;

	int64_t GetTotalHeals() const { return directHeals + critHeals; }
};

struct BattleData
{
	int     battleNumber    = 0;
	int64_t totalDamage     = 0;
	int64_t critDamage      = 0;
	int64_t dotDamage       = 0;
	int64_t petDamage       = 0;
	int64_t nonMeleeDamage  = 0;
	int64_t directHeals     = 0;
	int64_t critHeals       = 0;
	int     hitCount        = 0;
	float   durationSeconds = 0.0f;
	float   dps             = 0.0f;
	float   avgDamage       = 0.0f;
	std::unordered_map<int, TargetDamageData> targets;
	std::unordered_map<std::string, HealTargetData> healTargets;
};

struct MyDPSSettings
{
	bool  sortNewest       = true;
	bool  showType         = true;
	bool  showTarget       = true;
	bool  showMyMisses     = true;
	bool  showMissMe       = true;
	bool  showHitMe        = true;
	bool  showCritHeals    = true;
	bool  showDS           = true;
	bool  addPet           = true;
	bool  autoStart        = false;
	bool  spamClickThrough = true;
	int   displayTime      = 10;
	int   battleEndDelay   = 10;
	float fontScale        = 1.0f;
	float spamFontScale    = 1.0f;
	ImVec4 bgColor         = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);

	std::unordered_map<std::string, ImVec4> damageColors;

	void InitDefaultColors()
	{
		damageColors["crush"]           = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["kick"]            = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["bite"]            = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["bash"]            = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["hit"]             = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["pierce"]          = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["backstab"]        = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		damageColors["slash"]           = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["miss"]            = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		damageColors["missed-me"]       = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		damageColors["non-melee"]       = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
		damageColors["hit-by"]          = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		damageColors["crit"]            = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		damageColors["hit-by-non-melee"]= ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		damageColors["dShield"]         = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
		damageColors["critHeals"]       = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
		damageColors["dot"]             = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		damageColors["heal"]            = ImVec4(0.0f, 1.0f, 0.5f, 1.0f);
		damageColors["dmg-target"]      = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
		damageColors["heal-target"]     = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
		damageColors["dps"]             = ImVec4(1.0f, 0.9f, 0.4f, 1.0f);
		damageColors["duration"]        = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
		damageColors["avg"]             = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
		damageColors["total"]           = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		damageColors["pet"]             = ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
	}
};

inline const char* DamageTypeToString(DamageType type)
{
	switch (type)
	{
	case DamageType::Melee:          return "melee";
	case DamageType::NonMelee:       return "non-melee";
	case DamageType::DoT:            return "dot";
	case DamageType::DamageShield:   return "dShield";
	case DamageType::Crit:           return "crit";
	case DamageType::CritHeal:       return "critHeals";
	case DamageType::Miss:           return "miss";
	case DamageType::MissedMe:       return "missed-me";
	case DamageType::HitBy:          return "hit-by";
	case DamageType::HitByNonMelee:  return "hit-by-non-melee";
	case DamageType::PetMelee:       return "pet-melee";
	case DamageType::PetNonMelee:    return "pet-non-melee";
	case DamageType::DirectHeal:     return "heal";
	default:                         return "unknown";
	}
}

inline const char* DamageTypeToColorKey(DamageType type)
{
	switch (type)
	{
	case DamageType::NonMelee:       return "non-melee";
	case DamageType::DoT:            return "dot";
	case DamageType::DamageShield:   return "dShield";
	case DamageType::Crit:           return "crit";
	case DamageType::CritHeal:       return "critHeals";
	case DamageType::Miss:           return "miss";
	case DamageType::MissedMe:       return "missed-me";
	case DamageType::HitBy:          return "hit-by";
	case DamageType::HitByNonMelee:  return "hit-by-non-melee";
	case DamageType::PetMelee:       return "hit";
	case DamageType::PetNonMelee:    return "non-melee";
	case DamageType::DirectHeal:     return "heal";
	default:                         return "hit";
	}
}
