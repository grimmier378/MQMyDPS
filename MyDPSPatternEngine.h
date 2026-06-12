#pragma once

#include "MyDPSData.h"

#include <Blech/Blech.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct PatternDef
{
	std::string key;
	DamageType  type        = DamageType::Melee;
	std::string rawPattern;
	std::string pattern;
	bool        enabled     = true;
	unsigned int blechId    = 0;
};

class MyDPSPatternEngine
{
public:
	void Initialize(const std::string& charName);
	void Shutdown();

	void LoadPatterns(const std::string& serverName, const std::string& charName);
	void SavePatterns();

	void RegisterEvents();
	void UnregisterEvents();
	void RegisterPetEvents(const std::string& petName);
	void UnregisterPetEvents();

	std::optional<DamageRecord> Feed(const char* line, DWORD color);

	std::vector<PatternDef>& GetPatterns()    { return m_patterns; }
	std::vector<PatternDef>& GetPetPatterns() { return m_petPatterns; }
	const std::string& GetPatternsPath() const { return m_patternsPath; }

private:
	using BuilderFn = bool(MyDPSPatternEngine::*)(DamageRecord&, PBLECHVALUE);

	void GenerateDefaults();
	void InitBuilders();

	static std::string GetBlechValue(PBLECHVALUE pValues, const char* name);
	static int64_t     GetBlechInt(PBLECHVALUE pValues, const char* name);
	static DamageType  StringToDamageType(const std::string& s);
	static const char* DamageTypeToJsonString(DamageType type);

	void HandleMatch(unsigned int id, PBLECHVALUE pValues);
	static void CALLBACK BlechCallback(unsigned int id, void* pData, PBLECHVALUE pValues);
	static unsigned int CALLBACK BlechVarCallback(char* varName, char* value, size_t valueLen);

	bool BuildMelee(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildMiss(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildNonMelee(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildCrit(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildDeadlyStrike(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildDoT(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildDamageShield(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildHitBy(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildMissedMe(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildCritHeal(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildDirectHeal(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildHealedBy(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildHitByNonMelee(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildPetMelee(DamageRecord& rec, PBLECHVALUE pValues);
	bool BuildPetNonMelee(DamageRecord& rec, PBLECHVALUE pValues);

	std::unique_ptr<Blech>                          m_blech;
	std::vector<PatternDef>                         m_patterns;
	std::vector<PatternDef>                         m_petPatterns;
	std::unordered_map<unsigned int, PatternDef*>   m_blechEventMap;
	std::unordered_map<std::string, BuilderFn>      m_builders;

	std::string  m_charName;
	std::string  m_petName;
	std::string  m_serverName;
	std::string  m_patternsPath;

	bool         m_matched       = false;
	DamageRecord m_pendingRecord;
};
