#include <mq/Plugin.h>
#include "MQMyDPS.h"
#include "MyDPSParser.h"
#include "MyDPSRenderer.h"
#include "MyDPSTLO.h"

#include <filesystem>
#include <fmt/format.h>

PreSetup("MQMyDPS");
PLUGIN_VERSION(0.1);

MyDPSEngine* g_dpsEngine = nullptr;
static MyDPSRenderer s_renderer;

static void MyDPSCommand(PlayerClient*, const char* szLine);

static std::string FormatNumber(int64_t value)
{
	if (value >= 1000000)
		return fmt::format("{:.1f}m", value / 1000000.0);
	if (value >= 1000)
		return fmt::format("{:.1f}k", value / 1000.0);
	return fmt::format("{}", value);
}

PLUGIN_API void InitializePlugin()
{
	g_dpsEngine = new MyDPSEngine();
	g_dpsEngine->Initialize();
	AddCommand("/mydps", MyDPSCommand);
	RegisterMyDPSTLO();
}

PLUGIN_API void ShutdownPlugin()
{
	UnregisterMyDPSTLO();
	RemoveCommand("/mydps");
	g_dpsEngine->Shutdown();
	delete g_dpsEngine;
	g_dpsEngine = nullptr;
}

PLUGIN_API void SetGameState(int GameState)
{
	if (!g_dpsEngine)
		return;

	if (GameState == GAMESTATE_INGAME)
		g_dpsEngine->LoadCharacterSettings();
	else if (GameState == GAMESTATE_CHARSELECT)
		g_dpsEngine->UnloadCharacterSettings();
}

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	if (g_dpsEngine && g_dpsEngine->tracking)
		g_dpsEngine->ProcessChat(Line, Color);
	return false;
}

PLUGIN_API void OnPulse()
{
	if (!g_dpsEngine || !g_dpsEngine->tracking)
		return;

	static auto pulseTimer = std::chrono::steady_clock::now();
	if (std::chrono::steady_clock::now() < pulseTimer)
		return;
	pulseTimer = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);

	g_dpsEngine->TrackDoTCasting();
	g_dpsEngine->UpdateCombatState();
	g_dpsEngine->CleanExpiredRecords();
}

PLUGIN_API void OnUpdateImGui()
{
	if (!g_dpsEngine || GetGameState() != GAMESTATE_INGAME)
		return;

	s_renderer.RenderMainWindow(*g_dpsEngine);
	s_renderer.RenderConfigWindow(*g_dpsEngine);
	s_renderer.RenderCombatSpam(*g_dpsEngine);
	s_renderer.RenderFloatingText(*g_dpsEngine);
}

PLUGIN_API void OnBeginZone()
{
	if (g_dpsEngine)
	{
		g_dpsEngine->SaveCharacterSettings();
		g_dpsEngine->inCombat = false;
		g_dpsEngine->activeDoTs.clear();
		g_dpsEngine->fctManager.Clear();
	}
}

PLUGIN_API void OnRemoveSpawn(PSPAWNINFO pSpawn)
{
	if (g_dpsEngine && pSpawn)
		g_dpsEngine->RemoveActiveDoTs(pSpawn->SpawnID);
}

static void MyDPSCommand(PlayerClient*, const char* szLine)
{
	if (!g_dpsEngine)
		return;

	char arg[MAX_STRING] = {};
	GetArg(arg, szLine, 1);

	if (arg[0] == '\0')
	{
		g_dpsEngine->showMainWindow = !g_dpsEngine->showMainWindow;
		return;
	}

	if (ci_equals(arg, "start"))
	{
		g_dpsEngine->tracking = true;
		if (g_dpsEngine->sessionDamage == 0)
			g_dpsEngine->sessionStartTime = std::chrono::steady_clock::now();
		WriteChatf("\at[MQMyDPS]\ax Tracking started.");
		return;
	}

	if (ci_equals(arg, "stop"))
	{
		g_dpsEngine->tracking = false;
		WriteChatf("\at[MQMyDPS]\ax Tracking stopped.");
		return;
	}

	if (ci_equals(arg, "reset"))
	{
		g_dpsEngine->ResetAll();
		WriteChatf("\at[MQMyDPS]\ax All data reset.");
		return;
	}

	if (ci_equals(arg, "report"))
	{
		float dps = g_dpsEngine->GetSessionDPS();
		std::string report = fmt::format(
			"\at[MQMyDPS]\ax Session DPS: \ay{:.1f}\ax | Total: \ag{}\ax | Hits: \aw{}\ax",
			dps, FormatNumber(g_dpsEngine->sessionDamage), g_dpsEngine->sessionHitCount);
		WriteChatf("%s", report.c_str());
		g_dpsEngine->SendToMyChat(report);
		return;
	}

	if (ci_equals(arg, "config"))
	{
		g_dpsEngine->showConfigWindow = !g_dpsEngine->showConfigWindow;
		return;
	}

	if (ci_equals(arg, "show"))
	{
		g_dpsEngine->showMainWindow = true;
		return;
	}

	if (ci_equals(arg, "hide"))
	{
		g_dpsEngine->showMainWindow = false;
		return;
	}

	if (ci_equals(arg, "spam"))
	{
		g_dpsEngine->showCombatSpam = !g_dpsEngine->showCombatSpam;
		WriteChatf("\at[MQMyDPS]\ax Combat spam %s.", g_dpsEngine->showCombatSpam ? "shown" : "hidden");
		return;
	}

	if (ci_equals(arg, "lock"))
	{
		g_dpsEngine->settings.spamClickThrough = !g_dpsEngine->settings.spamClickThrough;
		WriteChatf("\at[MQMyDPS]\ax Spam window %s.", g_dpsEngine->settings.spamClickThrough ? "locked" : "unlocked");
		return;
	}

	if (ci_equals(arg, "fct"))
	{
		g_dpsEngine->settings.showFCT = !g_dpsEngine->settings.showFCT;
		WriteChatf("\at[MQMyDPS]\ax Floating combat text %s.", g_dpsEngine->settings.showFCT ? "enabled" : "disabled");
		return;
	}

	WriteChatf("\at[MQMyDPS]\ax Commands: start, stop, reset, report, show, hide, spam, lock, fct, config");
}

void MyDPSEngine::Initialize()
{
	m_parser = std::make_unique<MyDPSParser>();
	settings.InitDefaultColors();
	sessionStartTime = std::chrono::steady_clock::now();
}

void MyDPSEngine::Shutdown()
{
	SaveCharacterSettings();
	m_parser.reset();
}

void MyDPSEngine::LoadCharacterSettings()
{
	if (!pLocalPC || !pLocalPlayer)
		return;

	m_myChatLoaded = IsPluginLoaded("MQMyChat");
	charName = pLocalPC->Name;
	serverName = GetServerShortName();

	m_parser->SetCharName(charName);

	if (pLocalPlayer->PetID > 0)
	{
		if (auto* pPet = GetSpawnByID(pLocalPlayer->PetID))
			m_parser->SetPetName(pPet->DisplayedName);
	}

	std::string path = GetSettingsPath();
	if (path.empty())
		return;

	if (!std::filesystem::exists(path))
		return;

	settings.sortNewest       = GetPrivateProfileBool("Options", "SortNewest", true, path);
	settings.showType         = GetPrivateProfileBool("Options", "ShowType", true, path);
	settings.showTarget       = GetPrivateProfileBool("Options", "ShowTarget", true, path);
	settings.showMyMisses     = GetPrivateProfileBool("Options", "ShowMyMisses", true, path);
	settings.showMissMe       = GetPrivateProfileBool("Options", "ShowMissMe", true, path);
	settings.showHitMe        = GetPrivateProfileBool("Options", "ShowHitMe", true, path);
	settings.showCritHeals    = GetPrivateProfileBool("Options", "ShowCritHeals", true, path);
	settings.showDS           = GetPrivateProfileBool("Options", "ShowDS", true, path);
	settings.addPet           = GetPrivateProfileBool("Options", "AddPet", true, path);
	settings.autoStart        = GetPrivateProfileBool("Options", "AutoStart", false, path);
	settings.spamClickThrough = GetPrivateProfileBool("Options", "SpamClickThrough", true, path);
	settings.displayTime      = GetPrivateProfileInt("Options", "DisplayTime", 10, path);
	settings.battleEndDelay   = GetPrivateProfileInt("Options", "BattleEndDelay", 10, path);

	char buf[64] = {};
	GetPrivateProfileString("Options", "FontScale", "1.0", buf, sizeof(buf), path);
	settings.fontScale = static_cast<float>(atof(buf));
	GetPrivateProfileString("Options", "SpamFontScale", "1.0", buf, sizeof(buf), path);
	settings.spamFontScale = static_cast<float>(atof(buf));

	showMainWindow = GetPrivateProfileBool("Windows", "ShowMain", true, path);
	showConfigWindow = GetPrivateProfileBool("Windows", "ShowConfig", false, path);
	showCombatSpam = GetPrivateProfileBool("Windows", "ShowCombatSpam", true, path);

	settings.showFCT           = GetPrivateProfileBool("FCT", "Enabled", false, path);
	settings.showFCT_Melee     = GetPrivateProfileBool("FCT", "Melee", true, path);
	settings.showFCT_DD        = GetPrivateProfileBool("FCT", "DD", true, path);
	settings.showFCT_DoT       = GetPrivateProfileBool("FCT", "DoT", true, path);
	settings.showFCT_Pet       = GetPrivateProfileBool("FCT", "Pet", true, path);
	settings.showFCT_Crit      = GetPrivateProfileBool("FCT", "Crit", true, path);
	settings.showFCT_Heals     = GetPrivateProfileBool("FCT", "Heals", true, path);
	settings.showFCT_CritHeals = GetPrivateProfileBool("FCT", "CritHeals", true, path);
	settings.showFCT_HitBy     = GetPrivateProfileBool("FCT", "HitBy", true, path);
	GetPrivateProfileString("FCT", "Lifetime", "2.5", buf, sizeof(buf), path);
	settings.fctLifetime = static_cast<float>(atof(buf));
	GetPrivateProfileString("FCT", "BaseFontSize", "24.0", buf, sizeof(buf), path);
	settings.fctBaseFontSize = static_cast<float>(atof(buf));
	GetPrivateProfileString("FCT", "FontScale", "1.5", buf, sizeof(buf), path);
	settings.fctFontScale = static_cast<float>(atof(buf));
	GetPrivateProfileString("FCT", "ShadowOffset", "2.0", buf, sizeof(buf), path);
	settings.fctShadowOffset = static_cast<float>(atof(buf));

	if (settings.autoStart)
		tracking = true;

	WriteChatf("\at[MQMyDPS]\ax Settings loaded for %s.", charName.c_str());
}

void MyDPSEngine::SaveCharacterSettings()
{
	std::string path = GetSettingsPath();
	if (path.empty())
		return;

	std::filesystem::create_directories(std::filesystem::path(path).parent_path());

	WritePrivateProfileBool("Options", "SortNewest", settings.sortNewest, path);
	WritePrivateProfileBool("Options", "ShowType", settings.showType, path);
	WritePrivateProfileBool("Options", "ShowTarget", settings.showTarget, path);
	WritePrivateProfileBool("Options", "ShowMyMisses", settings.showMyMisses, path);
	WritePrivateProfileBool("Options", "ShowMissMe", settings.showMissMe, path);
	WritePrivateProfileBool("Options", "ShowHitMe", settings.showHitMe, path);
	WritePrivateProfileBool("Options", "ShowCritHeals", settings.showCritHeals, path);
	WritePrivateProfileBool("Options", "ShowDS", settings.showDS, path);
	WritePrivateProfileBool("Options", "AddPet", settings.addPet, path);
	WritePrivateProfileBool("Options", "AutoStart", settings.autoStart, path);
	WritePrivateProfileBool("Options", "SpamClickThrough", settings.spamClickThrough, path);
	WritePrivateProfileInt("Options", "DisplayTime", settings.displayTime, path);
	WritePrivateProfileInt("Options", "BattleEndDelay", settings.battleEndDelay, path);

	auto floatStr = [](float v) { return fmt::format("{:.2f}", v); };
	WritePrivateProfileString("Options", "FontScale", floatStr(settings.fontScale), path);
	WritePrivateProfileString("Options", "SpamFontScale", floatStr(settings.spamFontScale), path);

	WritePrivateProfileBool("Windows", "ShowMain", showMainWindow, path);
	WritePrivateProfileBool("Windows", "ShowConfig", showConfigWindow, path);
	WritePrivateProfileBool("Windows", "ShowCombatSpam", showCombatSpam, path);

	WritePrivateProfileBool("FCT", "Enabled", settings.showFCT, path);
	WritePrivateProfileBool("FCT", "Melee", settings.showFCT_Melee, path);
	WritePrivateProfileBool("FCT", "DD", settings.showFCT_DD, path);
	WritePrivateProfileBool("FCT", "DoT", settings.showFCT_DoT, path);
	WritePrivateProfileBool("FCT", "Pet", settings.showFCT_Pet, path);
	WritePrivateProfileBool("FCT", "Crit", settings.showFCT_Crit, path);
	WritePrivateProfileBool("FCT", "Heals", settings.showFCT_Heals, path);
	WritePrivateProfileBool("FCT", "CritHeals", settings.showFCT_CritHeals, path);
	WritePrivateProfileBool("FCT", "HitBy", settings.showFCT_HitBy, path);
	WritePrivateProfileString("FCT", "Lifetime", floatStr(settings.fctLifetime), path);
	WritePrivateProfileString("FCT", "BaseFontSize", floatStr(settings.fctBaseFontSize), path);
	WritePrivateProfileString("FCT", "FontScale", floatStr(settings.fctFontScale), path);
	WritePrivateProfileString("FCT", "ShadowOffset", floatStr(settings.fctShadowOffset), path);
}

void MyDPSEngine::UnloadCharacterSettings()
{
	if (charName.empty())
		return;

	SaveCharacterSettings();
	charName.clear();
	serverName.clear();
}

void MyDPSEngine::ProcessChat(const char* line, DWORD color)
{
	if (!m_parser)
		return;

	if (settings.addPet && pLocalPlayer && pLocalPlayer->PetID > 0)
	{
		if (auto* pPet = GetSpawnByID(pLocalPlayer->PetID))
			m_parser->SetPetName(pPet->DisplayedName);
	}
	else
	{
		m_parser->SetPetName("");
	}

	auto result = m_parser->Parse(line, color);
	if (!result.has_value())
		return;

	auto& rec = result.value();

	if (rec.isMiss && rec.type == DamageType::Miss && !settings.showMyMisses)
		return;
	if (rec.type == DamageType::MissedMe && !settings.showMissMe)
		return;
	if (rec.type == DamageType::HitBy && !settings.showHitMe)
		return;
	if (rec.type == DamageType::HitByNonMelee && !settings.showHitMe)
		return;
	if (rec.type == DamageType::CritHeal && !settings.showCritHeals)
		return;
	if (rec.type == DamageType::DamageShield && !settings.showDS)
		return;

	RecordDamage(rec);
}

void MyDPSEngine::RecordDamage(DamageRecord& record)
{
	record.sequence = ++sequenceCounter;

	if (record.targetName.empty() && record.type == DamageType::Crit && pTarget)
		record.targetName = pTarget->DisplayedName;

	bool isDamageOut = (record.type != DamageType::HitBy
		&& record.type != DamageType::HitByNonMelee
		&& record.type != DamageType::MissedMe
		&& record.type != DamageType::CritHeal
		&& record.type != DamageType::DirectHeal);

	if (isDamageOut && !record.isMiss && record.damage > 0)
	{
		if (!inCombat)
		{
			inCombat = true;
			m_combatEndPending = false;
			battleDamage = 0;
			battleCritDamage = 0;
			battleDotDamage = 0;
			battlePetDamage = 0;
			battleNonMeleeDmg = 0;
			battleDirectHeals = 0;
			battleCritHeals = 0;
			battleHitCount = 0;
			currentTargets.clear();
			currentHealTargets.clear();
			battleStartTime = std::chrono::steady_clock::now();
		}

		int spawnID = ResolveSpawnID(record);
		record.targetSpawnID = spawnID;

		sessionDamage += record.damage;
		sessionHitCount++;
		battleDamage += record.damage;
		battleHitCount++;

		if (record.type == DamageType::Crit)
			battleCritDamage += record.damage;
		if (record.type == DamageType::DoT)
			battleDotDamage += record.damage;
		if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
			battlePetDamage += record.damage;
		if (record.type == DamageType::NonMelee)
			battleNonMeleeDmg += record.damage;

		auto& target = currentTargets[spawnID];
		if (target.name.empty())
		{
			target.spawnID = spawnID;
			target.name = record.targetName;
			target.firstHit = record.timestamp;
		}
		target.totalDamage += record.damage;
		target.hitCount++;
		target.lastHit = record.timestamp;

		if (record.type == DamageType::Crit)
			target.critDamage += record.damage;
		if (record.type == DamageType::DoT)
			target.dotDamage += record.damage;
		if (record.type == DamageType::DamageShield)
			target.dsDamage += record.damage;
		if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
			target.petDamage += record.damage;
		if (record.type == DamageType::NonMelee)
			target.nonMeleeDamage += record.damage;
	}

	if (record.type == DamageType::DirectHeal && record.damage > 0)
	{
		if (inCombat)
		{
			battleDirectHeals += record.damage;
			auto& ht = currentHealTargets[record.targetName];
			if (ht.name.empty())
				ht.name = record.targetName;
			ht.directHeals += record.damage;
			ht.healCount++;
		}
	}

	if (record.type == DamageType::CritHeal && record.damage > 0)
	{
		if (inCombat)
			battleCritHeals += record.damage;
	}

	if (record.targetSpawnID <= 0)
	{
		bool isIncoming = (record.type == DamageType::HitBy
			|| record.type == DamageType::HitByNonMelee
			|| record.type == DamageType::MissedMe
			|| record.type == DamageType::DirectHeal
			|| record.type == DamageType::CritHeal);
		if (isIncoming && pLocalPlayer)
			record.targetSpawnID = pLocalPlayer->SpawnID;
	}

	fctManager.AddEntry(record, settings);
	recentRecords.push_back(std::move(record));
}

void MyDPSEngine::UpdateCombatState()
{
	if (!pLocalPC || !inCombat)
		return;

	bool inGameCombat = (GetCombatState() == eCombatState_Combat);

	if (!inGameCombat)
	{
		if (!m_combatEndPending)
		{
			m_combatEndPending = true;
			m_leftCombatTime = std::chrono::steady_clock::now();
		}
		else
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - m_leftCombatTime).count();
			if (elapsed >= settings.battleEndDelay)
				FinalizeBattle();
		}
	}
	else
	{
		m_combatEndPending = false;
	}
}

void MyDPSEngine::FinalizeBattle()
{
	if (battleDamage > 0)
	{
		BattleData battle;
		battle.battleNumber = ++battleCounter;
		battle.totalDamage = battleDamage;
		battle.critDamage = battleCritDamage;
		battle.dotDamage = battleDotDamage;
		battle.petDamage = battlePetDamage;
		battle.nonMeleeDamage = battleNonMeleeDmg;
		battle.directHeals = battleDirectHeals;
		battle.critHeals = battleCritHeals;
		battle.hitCount = battleHitCount;
		battle.durationSeconds = GetBattleDuration();
		battle.dps = battle.durationSeconds > 0
			? static_cast<float>(battle.totalDamage) / battle.durationSeconds
			: 0.0f;
		battle.avgDamage = battle.hitCount > 0
			? static_cast<float>(battle.totalDamage) / static_cast<float>(battle.hitCount)
			: 0.0f;
		battle.targets = std::move(currentTargets);
		battle.healTargets = std::move(currentHealTargets);

		SendToMyChat(fmt::format(
			"\at[MQMyDPS]\ax Battle \aw#{}\ax ended: DPS \ay{:.1f}\ax | Dmg \ag{}\ax | Hits \aw{}\ax | Dur \aw{:.0f}s\ax",
			battle.battleNumber, battle.dps, FormatNumber(battle.totalDamage), battle.hitCount, battle.durationSeconds));

		for (const auto& [id, tgt] : battle.targets)
		{
			std::string label = tgt.spawnID > 0
				? fmt::format("{} (\ap#{}\ax)", tgt.name, tgt.spawnID) : tgt.name;
			SendToMyChat(fmt::format(
				"  \ar{}\ax - DPS \ay{:.1f}\ax | Dmg \ag{}\ax | Hits \aw{}\ax",
				label, tgt.GetDPS(), FormatNumber(tgt.totalDamage), tgt.hitCount));
		}

		for (const auto& [name, ht] : battle.healTargets)
		{
			SendToMyChat(fmt::format(
				"  \aoHealed\ax \at{}\ax - \ag{}\ax (\aw{}\ax heals)",
				ht.name, FormatNumber(ht.GetTotalHeals()), ht.healCount));
		}

		battleHistory.push_back(std::move(battle));
	}

	inCombat = false;
	m_combatEndPending = false;
	battleDamage = 0;
	battleCritDamage = 0;
	battleDotDamage = 0;
	battlePetDamage = 0;
	battleNonMeleeDmg = 0;
	battleDirectHeals = 0;
	battleCritHeals = 0;
	battleHitCount = 0;
	currentTargets.clear();
	currentHealTargets.clear();
}

void MyDPSEngine::ResetAll()
{
	recentRecords.clear();
	currentTargets.clear();
	currentHealTargets.clear();
	battleHistory.clear();
	activeDoTs.clear();

	inCombat = false;
	m_combatEndPending = false;
	battleCounter = 0;
	sequenceCounter = 0;
	m_syntheticIDCounter = 0;
	m_lastCastSpellID = -1;
	m_lastCastTargetID = 0;

	battleDamage = 0;
	battleCritDamage = 0;
	battleDotDamage = 0;
	battlePetDamage = 0;
	battleNonMeleeDmg = 0;
	battleDirectHeals = 0;
	battleCritHeals = 0;
	battleHitCount = 0;

	sessionDamage = 0;
	sessionHitCount = 0;
	sessionStartTime = std::chrono::steady_clock::now();

	fctManager.Clear();
}

int MyDPSEngine::ResolveSpawnID(const DamageRecord& record)
{
	if (record.targetName.empty())
		return --m_syntheticIDCounter;

	switch (record.type)
	{
	case DamageType::Melee:
	case DamageType::NonMelee:
	case DamageType::Crit:
		if (pTarget && pTarget->DisplayedName == record.targetName)
			return pTarget->SpawnID;
		break;

	case DamageType::PetMelee:
	case DamageType::PetNonMelee:
		if (pLocalPlayer && pLocalPlayer->PetID > 0)
		{
			if (PlayerClient* pPet = GetSpawnByID(pLocalPlayer->PetID))
			{
				if (PlayerClient* pPetTarget = pPet->WhoFollowing)
				{
					if (pPetTarget->DisplayedName == record.targetName)
						return pPetTarget->SpawnID;
				}
			}
		}
		break;

	case DamageType::DoT:
	{
		int dotID = ResolveDoTSpawnID(record.targetName, record.spellName);
		if (dotID > 0)
			return dotID;
		if (pTarget && pTarget->DisplayedName == record.targetName)
			return pTarget->SpawnID;
		break;
	}

	case DamageType::DamageShield:
	default:
		break;
	}

	for (const auto& [id, data] : currentTargets)
	{
		if (data.name == record.targetName)
			return id;
	}

	int xtID = ResolveFromXTarget(record.targetName);
	if (xtID > 0)
		return xtID;

	return --m_syntheticIDCounter;
}

int MyDPSEngine::ResolveDoTSpawnID(const std::string& targetName, const std::string& spellName)
{
	for (const auto& dot : activeDoTs)
	{
		if (dot.targetName == targetName && (spellName.empty() || dot.spellName == spellName))
			return dot.targetSpawnID;
	}
	return 0;
}

int MyDPSEngine::ResolveFromXTarget(const std::string& targetName)
{
	if (!pLocalPC || !pLocalPC->pExtendedTargetList)
		return 0;

	int matchID = 0;
	int matchCount = 0;

	for (const ExtendedTargetSlot& slot : *pLocalPC->pExtendedTargetList)
	{
		if (slot.XTargetSlotStatus == eXTSlotEmpty || slot.SpawnID == 0)
			continue;

		if (slot.Name != targetName)
			continue;

		SPAWNINFO* pSpawn = GetSpawnByID(slot.SpawnID);
		if (!pSpawn || pSpawn->Type != SPAWN_NPC)
			continue;

		if (pSpawn->MasterID != 0)
		{
			SPAWNINFO* pMaster = GetSpawnByID(pSpawn->MasterID);
			if (pMaster && pMaster->Type == SPAWN_PLAYER)
				continue;
		}

		matchID = pSpawn->SpawnID;
		matchCount++;
	}

	if (matchCount == 1)
		return matchID;

	return 0;
}

void MyDPSEngine::TrackDoTCasting()
{
	if (!pLocalPlayer)
		return;

	if (pLocalPlayer->CastingData.SpellID != -1)
	{
		m_lastCastSpellID = pLocalPlayer->CastingData.SpellID;
		m_lastCastTargetID = pLocalPlayer->CastingData.TargetID;
	}
	else if (m_lastCastSpellID != -1)
	{
		EQ_Spell* pSpell = GetSpellByID(m_lastCastSpellID);
		if (pSpell && pSpell->IsDoTSpell() && pSpell->IsDetrimentalSpell())
		{
			PlayerClient* pSpawnTarget = GetSpawnByID(m_lastCastTargetID);
			if (pSpawnTarget)
			{
				ActiveDoT dot;
				dot.targetSpawnID = m_lastCastTargetID;
				dot.targetName = pSpawnTarget->DisplayedName;
				dot.spellName = pSpell->Name;
				dot.castTime = std::chrono::steady_clock::now();
				activeDoTs.push_back(std::move(dot));
			}
		}
		m_lastCastSpellID = -1;
		m_lastCastTargetID = 0;
	}

	auto now = std::chrono::steady_clock::now();
	activeDoTs.erase(
		std::remove_if(activeDoTs.begin(), activeDoTs.end(),
			[&now](const ActiveDoT& d) {
				auto age = std::chrono::duration_cast<std::chrono::minutes>(now - d.castTime).count();
				return age > 5;
			}),
		activeDoTs.end());
}

void MyDPSEngine::RemoveActiveDoTs(int spawnID)
{
	activeDoTs.erase(
		std::remove_if(activeDoTs.begin(), activeDoTs.end(),
			[spawnID](const ActiveDoT& d) { return d.targetSpawnID == spawnID; }),
		activeDoTs.end());
}

void MyDPSEngine::CleanExpiredRecords()
{
	auto now = std::chrono::steady_clock::now();
	auto maxAge = std::chrono::seconds(settings.displayTime);

	while (!recentRecords.empty())
	{
		auto age = now - recentRecords.front().timestamp;
		if (age > maxAge)
			recentRecords.pop_front();
		else
			break;
	}
}

float MyDPSEngine::GetSessionDPS() const
{
	if (sessionDamage == 0)
		return 0.0f;
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - sessionStartTime).count();
	return elapsed > 0 ? static_cast<float>(sessionDamage) / static_cast<float>(elapsed) : 0.0f;
}

float MyDPSEngine::GetBattleDPS() const
{
	if (!inCombat || battleDamage == 0)
		return 0.0f;
	float dur = GetBattleDuration();
	return dur > 0.0f ? static_cast<float>(battleDamage) / dur : 0.0f;
}

float MyDPSEngine::GetBattleDuration() const
{
	if (!inCombat)
		return 0.0f;
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - battleStartTime).count();
	return std::max(1.0f, elapsed / 1000.0f);
}

bool MyDPSEngine::IsMyChatLoaded() const
{
	return m_myChatLoaded;
}

void MyDPSEngine::SendToMyChat(const std::string& message)
{
	if (!m_myChatLoaded)
		return;

	char szBuffer[MAX_STRING] = {};
	sprintf_s(szBuffer, "${MyChat.Send[MyDPS,%s]}", message.c_str());
	ParseMacroData(szBuffer, MAX_STRING);
}

std::string MyDPSEngine::GetSettingsPath() const
{
	if (serverName.empty() || charName.empty())
		return {};

	std::string path = fmt::format("{}\\MQMyDPS\\{}\\{}.ini", gPathConfig, serverName, charName);
	return path;
}
