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
static void DrawMyDPS_MQSettingsPanel();

PLUGIN_API void InitializePlugin()
{
	g_dpsEngine = new MyDPSEngine();
	g_dpsEngine->Initialize();
	AddCommand("/mydps", MyDPSCommand);
	AddSettingsPanel("plugins/MyDPS", DrawMyDPS_MQSettingsPanel);
	RegisterMyDPSTLO();
}

PLUGIN_API void ShutdownPlugin()
{
	UnregisterMyDPSTLO();
	RemoveSettingsPanel("plugins/MyDPS");
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
	g_dpsEngine->RefreshAggregates();
}

PLUGIN_API void OnUpdateImGui()
{
	if (!g_dpsEngine || GetGameState() != GAMESTATE_INGAME)
		return;

	s_renderer.RenderMainWindow(*g_dpsEngine);
	s_renderer.RenderConfigWindow(*g_dpsEngine);
	s_renderer.RenderCombatSpam(*g_dpsEngine);
	s_renderer.RenderFloatingText(*g_dpsEngine);
	s_renderer.RenderIconPicker(*g_dpsEngine);
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

static void DrawMyDPS_MQSettingsPanel()
{
	if (!g_dpsEngine)
		return;

	s_renderer.RenderConfig(*g_dpsEngine);
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
		WriteChatf("\at[MQMyDPS]\ax Combat output %s.", g_dpsEngine->showCombatSpam ? "shown" : "hidden");
		return;
	}

	if (ci_equals(arg, "lock"))
	{
		g_dpsEngine->settings.spamClickThrough = !g_dpsEngine->settings.spamClickThrough;
		WriteChatf("\at[MQMyDPS]\ax Combat output window %s.", g_dpsEngine->settings.spamClickThrough ? "locked" : "unlocked");
		return;
	}

	if (ci_equals(arg, "fct"))
	{
		g_dpsEngine->settings.showFCT = !g_dpsEngine->settings.showFCT;
		WriteChatf("\at[MQMyDPS]\ax Floating combat text %s.", g_dpsEngine->settings.showFCT ? "enabled" : "disabled");
		return;
	}

	if (ci_equals(arg, "help"))
	{
		WriteChatf("\at[MQMyDPS]\ax --- Command Help ---");
		WriteChatf("  \ay/mydps\ax           - Toggle main window");
		WriteChatf("  \ay/mydps show\ax      - Show main window");
		WriteChatf("  \ay/mydps hide\ax      - Hide main window");
		WriteChatf("  \ay/mydps start\ax     - Start DPS tracking");
		WriteChatf("  \ay/mydps stop\ax      - Stop DPS tracking");
		WriteChatf("  \ay/mydps reset\ax     - Reset all data");
		WriteChatf("  \ay/mydps report\ax    - Print session DPS to chat");
		WriteChatf("  \ay/mydps config\ax    - Toggle config window");
		WriteChatf("  \ay/mydps spam\ax      - Toggle combat output window");
		WriteChatf("  \ay/mydps lock\ax      - Toggle combat output click-through");
		WriteChatf("  \ay/mydps fct\ax       - Toggle floating combat text");
		WriteChatf("  \ay/mydps help\ax      - Show this help");
		return;
	}

	WriteChatf("\at[MQMyDPS]\ax Unknown command '\ar%s\ax'. Use \ay/mydps help\ax for a list of commands.", arg);
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
	if (settings.fontScale < 0.5f || settings.fontScale > 2.0f) settings.fontScale = 1.0f;
	GetPrivateProfileString("Options", "SpamFontScale", "1.0", buf, sizeof(buf), path);
	settings.spamFontScale = static_cast<float>(atof(buf));
	if (settings.spamFontScale < 0.5f || settings.spamFontScale > 2.0f) settings.spamFontScale = 1.0f;
	settings.themeIdx = GetPrivateProfileInt("Options", "ThemeIdx", 10, path);

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
	settings.showFCT_DS        = GetPrivateProfileBool("FCT", "DS", true, path);
	settings.showFCT_Icons     = GetPrivateProfileBool("FCT", "Icons", true, path);
	settings.fctDistinctMelee  = GetPrivateProfileBool("FCT", "DistinctMelee", true, path);
	settings.fctUseSpellIcons  = GetPrivateProfileBool("FCT", "UseSpellIcons", false, path);
	GetPrivateProfileString("FCT", "IconScale", "1.0", buf, sizeof(buf), path);
	settings.fctIconScale = static_cast<float>(atof(buf));
	if (settings.fctIconScale < 0.1f || settings.fctIconScale > 1.0f) settings.fctIconScale = 1.0f;
	GetPrivateProfileString("FCT", "FloatDistance", "150.0", buf, sizeof(buf), path);
	settings.fctFloatDistance = static_cast<float>(atof(buf));
	if (settings.fctFloatDistance < 30.0f || settings.fctFloatDistance > 300.0f) settings.fctFloatDistance = 150.0f;
	GetPrivateProfileString("FCT", "ArcScale", "1.0", buf, sizeof(buf), path);
	settings.fctArcScale = static_cast<float>(atof(buf));
	if (settings.fctArcScale < 0.0f || settings.fctArcScale > 3.0f) settings.fctArcScale = 1.0f;
	GetPrivateProfileString("FCT", "Lifetime", "2.5", buf, sizeof(buf), path);
	settings.fctLifetime = static_cast<float>(atof(buf));
	if (settings.fctLifetime < 1.0f || settings.fctLifetime > 5.0f) settings.fctLifetime = 2.5f;
	GetPrivateProfileString("FCT", "BaseFontSize", "24.0", buf, sizeof(buf), path);
	settings.fctBaseFontSize = static_cast<float>(atof(buf));
	if (settings.fctBaseFontSize < 12.0f || settings.fctBaseFontSize > 48.0f) settings.fctBaseFontSize = 24.0f;
	GetPrivateProfileString("FCT", "FontScale", "1.5", buf, sizeof(buf), path);
	settings.fctFontScale = static_cast<float>(atof(buf));
	if (settings.fctFontScale < 0.5f || settings.fctFontScale > 3.0f) settings.fctFontScale = 1.5f;
	GetPrivateProfileString("FCT", "ShadowOffset", "2.0", buf, sizeof(buf), path);
	settings.fctShadowOffset = static_cast<float>(atof(buf));
	if (settings.fctShadowOffset < 0.0f || settings.fctShadowOffset > 5.0f) settings.fctShadowOffset = 2.0f;
	settings.fctBonePlayer = GetPrivateProfileInt("FCT", "BonePlayer", 11, path);
	settings.fctBoneOther  = GetPrivateProfileInt("FCT", "BoneOther",  20, path);

	settings.fctIconOverrides.clear();
	for (const auto& info : GetFCTTypeInfoList())
	{
		GetPrivateProfileString("FCTIcons", info.key, "-1", buf, sizeof(buf), path);
		int id = atoi(buf);
		if (id >= 0 || id == FCT_ICON_NONE)
			settings.fctIconOverrides[info.key] = { id, id >= FCT_ITEM_ICON_OFFSET };
	}

	for (auto& [key, color] : settings.damageColors)
	{
		MQColor defaultColor(color);
		MQColor loaded = GetPrivateProfileColor("Colors", key.c_str(), defaultColor, path);
		color = loaded.ToImColor();
	}
	MQColor defaultBg(settings.bgColor);
	settings.bgColor = GetPrivateProfileColor("Colors", "background", defaultBg, path).ToImColor();

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
	WritePrivateProfileInt("Options", "ThemeIdx", settings.themeIdx, path);

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
	WritePrivateProfileBool("FCT", "DS", settings.showFCT_DS, path);
	WritePrivateProfileBool("FCT", "Icons", settings.showFCT_Icons, path);
	WritePrivateProfileBool("FCT", "DistinctMelee", settings.fctDistinctMelee, path);
	WritePrivateProfileBool("FCT", "UseSpellIcons", settings.fctUseSpellIcons, path);
	WritePrivateProfileString("FCT", "IconScale", floatStr(settings.fctIconScale), path);
	WritePrivateProfileString("FCT", "FloatDistance", floatStr(settings.fctFloatDistance), path);
	WritePrivateProfileString("FCT", "ArcScale", floatStr(settings.fctArcScale), path);
	WritePrivateProfileString("FCT", "Lifetime", floatStr(settings.fctLifetime), path);
	WritePrivateProfileString("FCT", "BaseFontSize", floatStr(settings.fctBaseFontSize), path);
	WritePrivateProfileString("FCT", "FontScale", floatStr(settings.fctFontScale), path);
	WritePrivateProfileString("FCT", "ShadowOffset", floatStr(settings.fctShadowOffset), path);
	WritePrivateProfileInt("FCT", "BonePlayer", settings.fctBonePlayer, path);
	WritePrivateProfileInt("FCT", "BoneOther",  settings.fctBoneOther,  path);

	for (const auto& info : GetFCTTypeInfoList())
	{
		auto it = settings.fctIconOverrides.find(info.key);
		int id = (it != settings.fctIconOverrides.end()) ? it->second.iconID : -1;
		WritePrivateProfileString("FCTIcons", info.key, std::to_string(id).c_str(), path);
	}

	for (const auto& [key, color] : settings.damageColors)
		WritePrivateProfileColor("Colors", key.c_str(), MQColor(color), path);
	WritePrivateProfileColor("Colors", "background", MQColor(settings.bgColor), path);
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
			battleDsDamage = 0;
			battleDirectHeals = 0;
			battleCritHeals = 0;
			battleHitCount = 0;
			currentTargets.clear();
			currentHealTargets.clear();
			battleStartTime = std::chrono::steady_clock::now();
		}

		int spawnID = ResolveSpawnID(record);
		record.targetSpawnID = spawnID;

		if (record.type == DamageType::Crit)
		{
			battleCritDamage += record.damage;

			if (spawnID > 0)
			{
				auto& target = currentTargets[spawnID];
				if (target.name.empty())
				{
					target.spawnID = spawnID;
					target.name = record.targetName;
					target.firstHit = record.timestamp;
				}
				target.critDamage += record.damage;
				target.lastHit = record.timestamp;

				auto& sessionTarget = cachedSessionTargets[spawnID];
				if (sessionTarget.name.empty())
				{
					sessionTarget.spawnID = spawnID;
					sessionTarget.name = record.targetName;
				}
				sessionTarget.critDamage += record.damage;
			}

			aggregateDirty = true;
		}
		else
		{
			sessionDamage += record.damage;
			sessionHitCount++;
			battleDamage += record.damage;
			battleHitCount++;

			if (record.type == DamageType::DoT)
				battleDotDamage += record.damage;
			if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
				battlePetDamage += record.damage;
			if (record.type == DamageType::NonMelee)
				battleNonMeleeDmg += record.damage;
			if (record.type == DamageType::DamageShield)
				battleDsDamage += record.damage;

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

			if (record.type == DamageType::DoT)
				target.dotDamage += record.damage;
			if (record.type == DamageType::DamageShield)
				target.dsDamage += record.damage;
			if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
				target.petDamage += record.damage;
			if (record.type == DamageType::NonMelee)
				target.nonMeleeDamage += record.damage;

			auto& sessionTarget = cachedSessionTargets[spawnID];
			if (sessionTarget.name.empty())
			{
				sessionTarget.spawnID = spawnID;
				sessionTarget.name = record.targetName;
			}
			sessionTarget.totalDamage += record.damage;
			sessionTarget.hitCount++;
			if (record.type == DamageType::DoT)
				sessionTarget.dotDamage += record.damage;
			if (record.type == DamageType::DamageShield)
				sessionTarget.dsDamage += record.damage;
			if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
				sessionTarget.petDamage += record.damage;
			if (record.type == DamageType::NonMelee)
				sessionTarget.nonMeleeDamage += record.damage;

			aggregateDirty = true;
		}
	}

	if (record.type == DamageType::DirectHeal && record.damage > 0)
	{
		m_lastHealTarget = record.targetName;
		if (inCombat)
		{
			battleDirectHeals += record.damage;
			auto& ht = currentHealTargets[record.targetName];
			if (ht.name.empty())
				ht.name = record.targetName;
			ht.directHeals += record.damage;
			ht.healCount++;

			auto& sht = cachedSessionHeals[record.targetName];
			if (sht.name.empty())
				sht.name = record.targetName;
			sht.directHeals += record.damage;
			sht.healCount++;
			aggregateDirty = true;
		}
	}

	if (record.type == DamageType::CritHeal && record.damage > 0)
	{
		if (record.targetName.empty() && !m_lastHealTarget.empty())
			record.targetName = m_lastHealTarget;

		if (inCombat)
		{
			battleCritHeals += record.damage;

			auto& ht = currentHealTargets[record.targetName];
			if (ht.name.empty())
				ht.name = record.targetName;
			ht.critHeals += record.damage;

			auto& sht = cachedSessionHeals[record.targetName];
			if (sht.name.empty())
				sht.name = record.targetName;
			sht.critHeals += record.damage;
			aggregateDirty = true;
		}
	}

	if (record.targetSpawnID <= 0)
	{
		bool isIncoming = (record.type == DamageType::HitBy
			|| record.type == DamageType::HitByNonMelee
			|| record.type == DamageType::MissedMe);
		if (isIncoming && pLocalPlayer)
			record.targetSpawnID = pLocalPlayer->SpawnID;

		bool isHeal = (record.type == DamageType::DirectHeal
			|| record.type == DamageType::CritHeal);
		if (isHeal && pLocalPlayer)
		{
			if (!record.targetName.empty() && record.targetName != charName)
			{
				if (PlayerClient* pTarget = GetSpawnByName(record.targetName.c_str()))
					record.targetSpawnID = pTarget->SpawnID;
			}
			else
			{
				record.targetSpawnID = pLocalPlayer->SpawnID;
			}
		}
	}

	record.spellIconID = ResolveSpellIconID(record);
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
		battle.dsDamage = battleDsDamage;
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
	battleDsDamage = 0;
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
	m_lastCastSpellIcon = -1;
	m_lastCastTargetID = 0;

	battleDamage = 0;
	battleCritDamage = 0;
	battleDotDamage = 0;
	battlePetDamage = 0;
	battleNonMeleeDmg = 0;
	battleDsDamage = 0;
	battleDirectHeals = 0;
	battleCritHeals = 0;
	battleHitCount = 0;

	sessionDamage = 0;
	sessionHitCount = 0;
	sessionStartTime = std::chrono::steady_clock::now();

	cachedSessionTargets.clear();
	sortedSessionTargets.clear();
	cachedSessionHeals.clear();
	sortedSessionHeals.clear();
	aggregateDirty = true;

	fctManager.Clear();
}

void MyDPSEngine::RefreshAggregates()
{
	if (!aggregateDirty)
		return;
	aggregateDirty = false;

	sortedSessionTargets.clear();
	sortedSessionTargets.reserve(cachedSessionTargets.size());
	for (const auto& [id, data] : cachedSessionTargets)
		sortedSessionTargets.push_back(data);
	std::sort(sortedSessionTargets.begin(), sortedSessionTargets.end(),
		[](const TargetDamageData& a, const TargetDamageData& b) {
			return a.totalDamage > b.totalDamage;
		});

	sortedSessionHeals.clear();
	sortedSessionHeals.reserve(cachedSessionHeals.size());
	for (const auto& [name, data] : cachedSessionHeals)
		sortedSessionHeals.push_back(data);
	std::sort(sortedSessionHeals.begin(), sortedSessionHeals.end(),
		[](const HealTargetData& a, const HealTargetData& b) {
			return a.GetTotalHeals() > b.GetTotalHeals();
		});
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

		if (EQ_Spell* pCasting = GetSpellByID(m_lastCastSpellID))
			m_lastCastSpellIcon = pCasting->SpellIcon;
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
				dot.spellIconID = pSpell->SpellIcon;
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

int MyDPSEngine::ResolveSpellIconID(const DamageRecord& record) const
{
	std::string overrideKey;
	if (record.type == DamageType::Melee && settings.fctDistinctMelee)
		overrideKey = record.attackVerb;
	else if (record.type == DamageType::Melee)
		overrideKey = "hit";
	else if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
		overrideKey = "pet";
	else
		overrideKey = DamageTypeToColorKey(record.type);

	auto overIt = settings.fctIconOverrides.find(overrideKey);
	if (overIt != settings.fctIconOverrides.end())
	{
		if (overIt->second.iconID == FCT_ICON_NONE)
			return -1;
		if (overIt->second.iconID >= 0)
			return overIt->second.iconID;
	}

	switch (record.type)
	{
	case DamageType::DirectHeal:
	case DamageType::CritHeal:
		if (settings.fctUseSpellIcons && !record.spellName.empty())
		{
			if (EQ_Spell* pSpell = GetSpellByName(record.spellName))
				return pSpell->SpellIcon;
		}
		return 0;

	case DamageType::Melee:
		if (record.attackVerb == "kick")
			return 201;
		if (record.attackVerb == "bash")
			return 155;
		return 49;

	case DamageType::PetMelee:
		return 3;

	case DamageType::Crit:
		if (settings.fctUseSpellIcons && !record.spellName.empty())
		{
			if (EQ_Spell* pSpell = GetSpellByName(record.spellName))
				return pSpell->SpellIcon;
		}
		return 50;

	case DamageType::DoT:
		if (settings.fctUseSpellIcons && !record.spellName.empty())
		{
			if (EQ_Spell* pSpell = GetSpellByName(record.spellName))
				return pSpell->SpellIcon;

			for (const auto& dot : activeDoTs)
			{
				if (dot.spellName == record.spellName && dot.spellIconID >= 0)
					return dot.spellIconID;
			}
		}
		return -1;

	case DamageType::PetNonMelee:
		return 55;

	case DamageType::NonMelee:
		if (settings.fctUseSpellIcons && !record.spellName.empty())
		{
			if (EQ_Spell* pSpell = GetSpellByName(record.spellName))
				return pSpell->SpellIcon;
		}
		if (settings.fctUseSpellIcons && m_lastCastSpellIcon >= 0)
			return m_lastCastSpellIcon;
		return -1;

	default:
		return -1;
	}
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
