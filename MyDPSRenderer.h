#pragma once

#include <mq/Plugin.h>

#include <chrono>
#include <string>
#include <vector>

class MyDPSEngine;

class MyDPSRenderer
{
public:
	~MyDPSRenderer();

	void RenderCombatSpam(MyDPSEngine& engine);
	void RenderMainWindow(MyDPSEngine& engine);
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
	void EnsurePickerAnimations();

	bool m_showDpsGraph = true;
	bool m_showDmgGraph = true;
	bool m_showBarChart = true;
	bool m_showPieChart = false;

	GraphScrollState m_dpsScroll;
	GraphScrollState m_dmgScroll;
	GraphScrollState m_barScroll;

	int m_cachedBattleCount = -1;
	std::vector<float> m_gNums, m_gDps, m_gMelee, m_gDD, m_gDots, m_gPets;
	std::vector<float> m_gCrits, m_gHeals, m_gCritHeals;
	int m_gTotalCached = 0;

	std::chrono::steady_clock::time_point m_lastFCTUpdate;

	bool        m_iconPickerOpen      = false;
	std::string m_iconPickerKey;
	std::string m_iconPickerLabel;
	int         m_iconPickerPage      = 1;
	bool        m_iconPickerShowItems = false;
	float       m_iconPickerScale     = 1.0f;

	CTextureAnimation* m_pPickerSpellAnim = nullptr;
	CTextureAnimation* m_pPickerItemAnim  = nullptr;
};
