#include "MyDPSPatternEngine.h"
#include "MQMyDPS.h"

#include <mq/Plugin.h>

#include <filesystem>
#include <fstream>
#include <charconv>
#include <fmt/format.h>

#include "json.hpp"

using json = nlohmann::json;

static constexpr int PATTERNS_VERSION = 2;

void MyDPSPatternEngine::Initialize(const std::string& charName)
{
	m_charName = charName;
	m_blech = std::make_unique<Blech>('#', '|', BlechVarCallback);
	InitBuilders();
}

void MyDPSPatternEngine::Shutdown()
{
	UnregisterPetEvents();
	UnregisterEvents();
	m_blech.reset();
	m_patterns.clear();
	m_petPatterns.clear();
	m_blechEventMap.clear();
}

void MyDPSPatternEngine::InitBuilders()
{
	m_builders["melee"]              = &MyDPSPatternEngine::BuildMelee;
	m_builders["miss"]               = &MyDPSPatternEngine::BuildMiss;
	m_builders["non_melee"]          = &MyDPSPatternEngine::BuildNonMelee;
	m_builders["crit"]               = &MyDPSPatternEngine::BuildCrit;
	m_builders["crit_blast"]         = &MyDPSPatternEngine::BuildCrit;
	m_builders["deadly_strike"]      = &MyDPSPatternEngine::BuildDeadlyStrike;
	m_builders["dot"]                = &MyDPSPatternEngine::BuildDoT;
	m_builders["damage_shield"]      = &MyDPSPatternEngine::BuildDamageShield;
	m_builders["hit_by"]             = &MyDPSPatternEngine::BuildHitBy;
	m_builders["missed_me"]          = &MyDPSPatternEngine::BuildMissedMe;
	m_builders["crit_heal"]          = &MyDPSPatternEngine::BuildCritHeal;
	m_builders["direct_heal"]        = &MyDPSPatternEngine::BuildDirectHeal;
	m_builders["direct_heal_simple"] = &MyDPSPatternEngine::BuildDirectHeal;
	m_builders["healed_by"]          = &MyDPSPatternEngine::BuildHealedBy;
	m_builders["hit_by_non_melee"]   = &MyDPSPatternEngine::BuildHitByNonMelee;
	m_builders["pet_melee"]          = &MyDPSPatternEngine::BuildPetMelee;
	m_builders["pet_non_melee"]      = &MyDPSPatternEngine::BuildPetNonMelee;
}

unsigned int CALLBACK MyDPSPatternEngine::BlechVarCallback(char*, char*, size_t)
{
	return 0;
}

void CALLBACK MyDPSPatternEngine::BlechCallback(unsigned int id, void* pData, PBLECHVALUE pValues)
{
	if (!pData)
	{
		return;
	}

	auto* pe = static_cast<MyDPSPatternEngine*>(pData);
	pe->HandleMatch(id, pValues);
}

void MyDPSPatternEngine::HandleMatch(unsigned int id, PBLECHVALUE pValues)
{
	if (m_matched)
	{
		return;
	}

	auto it = m_blechEventMap.find(id);
	if (it == m_blechEventMap.end())
	{
		return;
	}

	PatternDef* pDef = it->second;
	auto builderIt = m_builders.find(pDef->key);
	if (builderIt == m_builders.end())
	{
		return;
	}

	m_pendingRecord = DamageRecord{};
	m_pendingRecord.timestamp = std::chrono::steady_clock::now();

	BuilderFn builder = builderIt->second;
	m_matched = (this->*builder)(m_pendingRecord, pValues);
}

std::optional<DamageRecord> MyDPSPatternEngine::Feed(const char* line, DWORD color)
{
	if (!line || !line[0] || !m_blech)
	{
		return std::nullopt;
	}

	m_matched = false;
	m_blech->Feed(line, strlen(line));

	if (!m_matched)
	{
		return std::nullopt;
	}

	return std::move(m_pendingRecord);
}

std::string MyDPSPatternEngine::GetBlechValue(PBLECHVALUE pValues, const char* name)
{
	while (pValues)
	{
		if (pValues->Name == name)
		{
			return pValues->Value;
		}
		pValues = pValues->pNext;
	}
	return {};
}

int64_t MyDPSPatternEngine::GetBlechInt(PBLECHVALUE pValues, const char* name)
{
	std::string val = GetBlechValue(pValues, name);
	if (val.empty())
	{
		return 0;
	}

	int64_t result = 0;
	auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);
	if (ec != std::errc())
	{
		return 0;
	}
	return result;
}

DamageType MyDPSPatternEngine::StringToDamageType(const std::string& s)
{
	if (s == "Melee")
	{
		return DamageType::Melee;
	}
	if (s == "NonMelee")
	{
		return DamageType::NonMelee;
	}
	if (s == "DoT")
	{
		return DamageType::DoT;
	}
	if (s == "DamageShield")
	{
		return DamageType::DamageShield;
	}
	if (s == "Crit")
	{
		return DamageType::Crit;
	}
	if (s == "CritHeal")
	{
		return DamageType::CritHeal;
	}
	if (s == "Miss")
	{
		return DamageType::Miss;
	}
	if (s == "MissedMe")
	{
		return DamageType::MissedMe;
	}
	if (s == "HitBy")
	{
		return DamageType::HitBy;
	}
	if (s == "HitByNonMelee")
	{
		return DamageType::HitByNonMelee;
	}
	if (s == "PetMelee")
	{
		return DamageType::PetMelee;
	}
	if (s == "PetNonMelee")
	{
		return DamageType::PetNonMelee;
	}
	if (s == "DirectHeal")
	{
		return DamageType::DirectHeal;
	}
	return DamageType::Melee;
}

const char* MyDPSPatternEngine::DamageTypeToJsonString(DamageType type)
{
	switch (type)
	{
	case DamageType::Melee:         return "Melee";
	case DamageType::NonMelee:      return "NonMelee";
	case DamageType::DoT:           return "DoT";
	case DamageType::DamageShield:  return "DamageShield";
	case DamageType::Crit:          return "Crit";
	case DamageType::CritHeal:      return "CritHeal";
	case DamageType::Miss:          return "Miss";
	case DamageType::MissedMe:      return "MissedMe";
	case DamageType::HitBy:         return "HitBy";
	case DamageType::HitByNonMelee: return "HitByNonMelee";
	case DamageType::PetMelee:      return "PetMelee";
	case DamageType::PetNonMelee:   return "PetNonMelee";
	case DamageType::DirectHeal:    return "DirectHeal";
	default:                        return "Melee";
	}
}

void MyDPSPatternEngine::GenerateDefaults()
{
	m_patterns.clear();
	m_petPatterns.clear();

	auto add = [](std::vector<PatternDef>& vec, const std::string& key, DamageType type, const std::string& pattern) {
		PatternDef def;
		def.key = key;
		def.type = type;
		def.rawPattern = pattern;
		def.enabled = true;
		vec.push_back(std::move(def));
	};

	add(m_patterns, "melee",              DamageType::Melee,         "You #verb# #target# for #damage# points of damage.");
	add(m_patterns, "miss",               DamageType::Miss,          "You try to #verb# #target#, but miss!");
	add(m_patterns, "non_melee",          DamageType::NonMelee,      "{CharName} hit #target# for #damage# points of non-melee damage.");
	add(m_patterns, "crit",               DamageType::Crit,          "#*#You score a critical hit!#*#(#damage#)");
	add(m_patterns, "crit_blast",         DamageType::Crit,          "#*#You deliver a critical blast!#*#(#damage#)");
	add(m_patterns, "deadly_strike",      DamageType::Crit,          "#*#{CharName} scores a Deadly Strike!#*#(#damage#)");
	add(m_patterns, "dot",                DamageType::DoT,           "#target# has taken #damage# damage from your #spell#.");
	add(m_patterns, "damage_shield",      DamageType::DamageShield,  "#target# was hit by non-melee for #damage# points of damage.");
	add(m_patterns, "hit_by",             DamageType::HitBy,         "#attacker# #verb# YOU for #damage# points of damage.");
	add(m_patterns, "missed_me",          DamageType::MissedMe,      "#attacker# tries to #verb# YOU, but misses!");
	add(m_patterns, "crit_heal",          DamageType::CritHeal,      "#*#You perform an exceptional heal!#*#(#damage#)");
	add(m_patterns, "direct_heal",        DamageType::DirectHeal,    "You have healed #target# for #damage# hit points with your #spell#.");
	add(m_patterns, "direct_heal_simple", DamageType::DirectHeal,    "You have healed #target# for #damage# points.");
	add(m_patterns, "healed_by",          DamageType::DirectHeal,    "#healer# healed you for #damage# hit points by #spell#.");
	add(m_patterns, "hit_by_non_melee",   DamageType::HitByNonMelee, "#attacker# hit YOU for #damage# points of non-melee damage.");

	add(m_petPatterns, "pet_melee",     DamageType::PetMelee,     "{PetName} #verb# #target# for #damage# points of damage.");
	add(m_petPatterns, "pet_non_melee", DamageType::PetNonMelee,  "{PetName} hit #target# for #damage# points of non-melee damage.");
}

void MyDPSPatternEngine::LoadPatterns(const std::string& serverName, const std::string& charName)
{
	m_charName = charName;
	m_serverName = serverName;
	m_patternsPath = fmt::format("{}\\MQMyDPS\\{}\\Patterns.json", gPathConfig, serverName);

	if (!std::filesystem::exists(m_patternsPath))
	{
		GenerateDefaults();
		SavePatterns();
		WriteChatf("\at[MQMyDPS]\ax Default patterns generated: %s", m_patternsPath.c_str());
		return;
	}

	try
	{
		std::ifstream file(m_patternsPath);
		json j = json::parse(file);

		int fileVersion = j.value("version", 1);
		if (fileVersion < PATTERNS_VERSION)
		{
			file.close();

			std::filesystem::path backupPath = std::filesystem::path(m_patternsPath).replace_extension(
				fmt::format("v{}.bak.json", fileVersion));
			std::error_code ec;
			std::filesystem::rename(m_patternsPath, backupPath, ec);
			if (ec)
			{
				WriteChatf("\ar[MQMyDPS]\ax Failed to back up patterns before upgrade: %s", ec.message().c_str());
			}
			else
			{
				WriteChatf("\at[MQMyDPS]\ax Backed up existing patterns to: %s", backupPath.string().c_str());
			}

			GenerateDefaults();
			SavePatterns();
			WriteChatf("\at[MQMyDPS]\ax Patterns upgraded from v%d to v%d: %s", fileVersion, PATTERNS_VERSION, m_patternsPath.c_str());
			return;
		}

		m_patterns.clear();
		m_petPatterns.clear();

		if (j.contains("patterns") && j["patterns"].is_object())
		{
			for (auto& [key, val] : j["patterns"].items())
			{
				PatternDef def;
				def.key = key;
				def.type = StringToDamageType(val.value("type", "Melee"));
				def.rawPattern = val.value("pattern", "");
				def.enabled = val.value("enabled", true);
				m_patterns.push_back(std::move(def));
			}
		}

		if (j.contains("pet_patterns") && j["pet_patterns"].is_object())
		{
			for (auto& [key, val] : j["pet_patterns"].items())
			{
				PatternDef def;
				def.key = key;
				def.type = StringToDamageType(val.value("type", "PetMelee"));
				def.rawPattern = val.value("pattern", "");
				def.enabled = val.value("enabled", true);
				m_petPatterns.push_back(std::move(def));
			}
		}

		WriteChatf("\at[MQMyDPS]\ax Loaded %d patterns + %d pet patterns from JSON.",
			static_cast<int>(m_patterns.size()), static_cast<int>(m_petPatterns.size()));
	}
	catch (const json::exception& e)
	{
		WriteChatf("\ar[MQMyDPS]\ax Failed to parse patterns JSON: %s", e.what());
		GenerateDefaults();
	}
}

void MyDPSPatternEngine::SavePatterns()
{
	std::filesystem::create_directories(std::filesystem::path(m_patternsPath).parent_path());

	json j;
	j["version"] = PATTERNS_VERSION;

	json pats = json::object();
	for (const auto& def : m_patterns)
	{
		pats[def.key] = {
			{"type",    DamageTypeToJsonString(def.type)},
			{"pattern", def.rawPattern},
			{"enabled", def.enabled}
		};
	}
	j["patterns"] = pats;

	json petPats = json::object();
	for (const auto& def : m_petPatterns)
	{
		petPats[def.key] = {
			{"type",    DamageTypeToJsonString(def.type)},
			{"pattern", def.rawPattern},
			{"enabled", def.enabled}
		};
	}
	j["pet_patterns"] = petPats;

	std::ofstream file(m_patternsPath);
	file << j.dump(4);
}

void MyDPSPatternEngine::RegisterEvents()
{
	if (!m_blech)
	{
		return;
	}

	for (auto& def : m_patterns)
	{
		if (!def.enabled || def.rawPattern.empty())
		{
			continue;
		}

		def.pattern = mq::replace(def.rawPattern, "{CharName}", m_charName);
		unsigned int id = m_blech->AddEvent(def.pattern.c_str(), BlechCallback, this);
		def.blechId = id;
		m_blechEventMap[id] = &def;
	}
}

void MyDPSPatternEngine::UnregisterEvents()
{
	if (!m_blech)
	{
		return;
	}

	for (auto& def : m_patterns)
	{
		if (def.blechId != 0)
		{
			m_blech->RemoveEvent(def.blechId);
			m_blechEventMap.erase(def.blechId);
			def.blechId = 0;
		}
	}
}

void MyDPSPatternEngine::RegisterPetEvents(const std::string& petName)
{
	if (!m_blech || petName.empty())
	{
		return;
	}

	m_petName = petName;

	for (auto& def : m_petPatterns)
	{
		if (!def.enabled || def.rawPattern.empty())
		{
			continue;
		}

		def.pattern = mq::replace(def.rawPattern, "{PetName}", m_petName);
		def.pattern = mq::replace(def.pattern, "{CharName}", m_charName);
		unsigned int id = m_blech->AddEvent(def.pattern.c_str(), BlechCallback, this);
		def.blechId = id;
		m_blechEventMap[id] = &def;
	}
}

void MyDPSPatternEngine::UnregisterPetEvents()
{
	if (!m_blech)
	{
		return;
	}

	for (auto& def : m_petPatterns)
	{
		if (def.blechId != 0)
		{
			m_blech->RemoveEvent(def.blechId);
			m_blechEventMap.erase(def.blechId);
			def.blechId = 0;
		}
	}

	m_petName.clear();
}

bool MyDPSPatternEngine::BuildMelee(DamageRecord& rec, PBLECHVALUE pValues)
{
	std::string verb = GetBlechValue(pValues, "verb");
	if (verb == "have")
	{
		return false;
	}

	rec.type = DamageType::Melee;
	rec.attackVerb = std::move(verb);
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildMiss(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::Miss;
	rec.attackVerb = GetBlechValue(pValues, "verb");
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = 0;
	rec.isMiss = true;
	return true;
}

bool MyDPSPatternEngine::BuildNonMelee(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::NonMelee;
	rec.attackVerb = "non-melee";
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildCrit(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::Crit;
	rec.attackVerb = "crit";
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildDeadlyStrike(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::Crit;
	rec.attackVerb = "deadly";
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildDoT(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::DoT;
	rec.attackVerb = "dot";
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	rec.spellName = GetBlechValue(pValues, "spell");

	if (!rec.spellName.empty() && rec.spellName.back() == '.')
	{
		rec.spellName.pop_back();
	}
	return true;
}

bool MyDPSPatternEngine::BuildDamageShield(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::DamageShield;
	rec.attackVerb = "dShield";
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildHitBy(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::HitBy;
	rec.targetName = GetBlechValue(pValues, "attacker");
	rec.attackVerb = GetBlechValue(pValues, "verb");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildMissedMe(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::MissedMe;
	rec.targetName = GetBlechValue(pValues, "attacker");
	rec.attackVerb = GetBlechValue(pValues, "verb");
	rec.damage = 0;
	rec.isMiss = true;
	return true;
}

bool MyDPSPatternEngine::BuildCritHeal(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::CritHeal;
	rec.attackVerb = "critHeal";
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildDirectHeal(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::DirectHeal;
	rec.attackVerb = "heal";
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	rec.spellName = GetBlechValue(pValues, "spell");

	if (!rec.spellName.empty() && rec.spellName.back() == '.')
	{
		rec.spellName.pop_back();
	}
	return true;
}

bool MyDPSPatternEngine::BuildHealedBy(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::DirectHeal;
	rec.attackVerb = "heal";
	rec.targetName = m_charName;
	rec.damage = GetBlechInt(pValues, "damage");
	rec.spellName = GetBlechValue(pValues, "spell");

	if (!rec.spellName.empty() && rec.spellName.back() == '.')
	{
		rec.spellName.pop_back();
	}

	if (rec.damage <= 0)
	{
		return false;
	}
	return true;
}

bool MyDPSPatternEngine::BuildHitByNonMelee(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::HitByNonMelee;
	rec.attackVerb = "non-melee";
	rec.targetName = GetBlechValue(pValues, "attacker");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildPetMelee(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::PetMelee;
	std::string verb = GetBlechValue(pValues, "verb");
	rec.attackVerb = m_petName + " " + verb;
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}

bool MyDPSPatternEngine::BuildPetNonMelee(DamageRecord& rec, PBLECHVALUE pValues)
{
	rec.type = DamageType::PetNonMelee;
	rec.attackVerb = m_petName + " non-melee";
	rec.targetName = GetBlechValue(pValues, "target");
	rec.damage = GetBlechInt(pValues, "damage");
	return true;
}
