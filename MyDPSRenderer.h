#pragma once

#include "MyDPSActors.h"

#include <mq/Plugin.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class MyDPSEngine;

class MyDPSRenderer
{
public:
	~MyDPSRenderer();

	void RenderCombatSpam(MyDPSEngine& engine);
	void RenderMainWindow(MyDPSEngine& engine);
	void RenderPopOutWindows(MyDPSEngine& engine);
	void RenderConfigWindow(MyDPSEngine& engine);
	void RenderFloatingText(MyDPSEngine& engine);
	void RenderIconPicker(MyDPSEngine& engine);
	void RenderConfig(MyDPSEngine& engine);

private:
	struct GraphScrollState
	{
		int  offset     = 0;
		bool autoScroll = true;
	};

	enum DpsTab
	{
		TAB_CURRENT = 0,
		TAB_HISTORY,
		TAB_TARGETS,
		TAB_HEALING,
		TAB_GRAPHS,
		TAB_GROUP,
		TAB_COUNT
	};

public:
	struct EncounterChar
	{
		const PeerRecord* peer = nullptr;
		int64_t damage     = 0;
		int64_t heals      = 0;
		float   activeDurS = 0.0f;
		int     battles    = 0;
	};

	struct Encounter
	{
		int64_t startMs = 0;
		int64_t endMs   = 0;
		std::vector<EncounterChar> chars;

		float DurationS() const;
	};

private:
	static const char* TabName(int tab);
	void RenderTabBody(MyDPSEngine& engine, int tab);

	const std::vector<Encounter>& EnsureEncounters(MyDPSEngine& engine, const std::vector<const PeerRecord*>& peers);

	void RenderCurrentBattle(MyDPSEngine& engine);
	void RenderHistory(MyDPSEngine& engine);
	void RenderTargets(MyDPSEngine& engine);
	void RenderGraphs(MyDPSEngine& engine);
	void RenderDPSGraph(MyDPSEngine& engine);
	void RenderDamageGraph(MyDPSEngine& engine);
	void RenderBarChart(MyDPSEngine& engine);
	void RenderScrollBar(const char* id, GraphScrollState& scroll, int totalBattles, int maxBack);
	void RebuildGraphCache(MyDPSEngine& engine);
	void RenderPieChart(MyDPSEngine& engine);
	void RenderHealing(MyDPSEngine& engine);

	void RenderPeerLiveMeter(MyDPSEngine& engine);
	void RenderGroup(MyDPSEngine& engine);
	void RenderGroupLive(MyDPSEngine& engine, const std::vector<const PeerRecord*>& peers);
	void RenderGroupHistory(MyDPSEngine& engine, const std::vector<const PeerRecord*>& peers);
	void RenderGroupGraphs(MyDPSEngine& engine, const std::vector<const PeerRecord*>& peers);

	void EnsurePickerAnimations();

	bool m_showDpsGraph = true;
	bool m_showDmgGraph = true;
	bool m_showBarChart = true;
	bool m_showPieChart = false;

	bool m_tabPopOut[TAB_COUNT] = {};

	int m_mainTab     = 0;
	int m_groupSubTab = 0;
	int m_configTab   = 0;

	GraphScrollState m_dpsScroll;
	GraphScrollState m_dmgScroll;
	GraphScrollState m_barScroll;

	int m_cachedBattleCount = -1;
	int m_cachedResetGen    = -1;
	std::vector<float> m_gNums, m_gDps, m_gMelee, m_gDD, m_gDots, m_gPets;
	std::vector<float> m_gCrits, m_gHeals, m_gCritHeals;
	int m_gTotalCached = 0;

	std::vector<Encounter> m_encCache;
	int      m_encCachedResetGen   = -1;
	size_t   m_encCachedPeerCount  = static_cast<size_t>(-1);
	uint64_t m_encCachedBattleSum  = static_cast<uint64_t>(-1);
	int      m_encCachedFilterMode = -1;

	std::chrono::steady_clock::time_point m_lastFCTUpdate = std::chrono::steady_clock::now();

	bool        m_iconPickerOpen      = false;
	std::string m_iconPickerKey;
	std::string m_iconPickerLabel;
	int         m_iconPickerPage      = 1;
	bool        m_iconPickerShowItems = false;
	float       m_iconPickerScale     = 1.0f;

	CTextureAnimation* m_pPickerSpellAnim = nullptr;
	CTextureAnimation* m_pPickerItemAnim  = nullptr;
};
