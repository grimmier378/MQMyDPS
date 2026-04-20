#pragma once

#include "MyDPSData.h"

#include <chrono>
#include <deque>
#include <unordered_map>
#include <vector>

class MyDPSParser;

class MyDPSEngine
{
public:
	void Initialize();
	void Shutdown();

	void LoadCharacterSettings();
	void SaveCharacterSettings();
	void UnloadCharacterSettings();

	void ProcessChat(const char* line, DWORD color);
	void UpdateCombatState();
	void CleanExpiredRecords();

	void RecordDamage(DamageRecord& record);
	void FinalizeBattle();
	void ResetAll();
	void TrackDoTCasting();
	void RemoveActiveDoTs(int spawnID);

	int ResolveSpawnID(const DamageRecord& record);
	int ResolveDoTSpawnID(const std::string& targetName, const std::string& spellName);
	int ResolveFromXTarget(const std::string& targetName);

	MyDPSSettings                                       settings;
	std::deque<DamageRecord>                            recentRecords;
	std::unordered_map<int, TargetDamageData>            currentTargets;
	std::vector<ActiveDoT>                              activeDoTs;
	std::vector<BattleData>                             battleHistory;
	std::unordered_map<std::string, HealTargetData>     currentHealTargets;

	std::string charName;
	std::string serverName;

	bool showMainWindow    = true;
	bool showConfigWindow  = false;
	bool showCombatSpam    = true;
	bool tracking          = false;

	bool inCombat          = false;
	int  battleCounter     = 0;
	int  sequenceCounter   = 0;

	int64_t battleDamage      = 0;
	int64_t battleCritDamage  = 0;
	int64_t battleDotDamage   = 0;
	int64_t battlePetDamage   = 0;
	int64_t battleNonMeleeDmg = 0;
	int64_t battleDirectHeals = 0;
	int64_t battleCritHeals   = 0;
	int     battleHitCount    = 0;

	int64_t sessionDamage     = 0;
	int     sessionHitCount   = 0;
	std::chrono::steady_clock::time_point sessionStartTime;
	std::chrono::steady_clock::time_point battleStartTime;

	float GetSessionDPS() const;
	float GetBattleDPS() const;
	float GetBattleDuration() const;

	bool  IsMyChatLoaded() const;
	void  SendToMyChat(const std::string& message);

private:
	std::unique_ptr<MyDPSParser>            m_parser;
	std::chrono::steady_clock::time_point   m_leftCombatTime;
	bool                                    m_combatEndPending = false;

	int      m_lastCastSpellID   = -1;
	uint32_t m_lastCastTargetID  = 0;
	int      m_syntheticIDCounter = 0;
	bool     m_myChatLoaded      = false;

	std::string GetSettingsPath() const;
};

extern MyDPSEngine* g_dpsEngine;
