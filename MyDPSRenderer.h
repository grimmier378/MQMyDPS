#pragma once

#include <chrono>
#include <vector>

class MyDPSEngine;

class MyDPSRenderer
{
public:
	void RenderCombatSpam(MyDPSEngine& engine);
	void RenderMainWindow(MyDPSEngine& engine);
	void RenderConfigWindow(MyDPSEngine& engine);
	void RenderFloatingText(MyDPSEngine& engine);

private:
	void RenderCurrentBattle(MyDPSEngine& engine);
	void RenderHistory(MyDPSEngine& engine);
	void RenderTargets(MyDPSEngine& engine);
	void RenderGraphs(MyDPSEngine& engine);
	void RenderLineGraph(MyDPSEngine& engine);
	void RenderPieChart(MyDPSEngine& engine);
	void RenderHealing(MyDPSEngine& engine);
	void RenderConfig(MyDPSEngine& engine);

	bool m_showPieChart  = false;
	bool m_showLineGraph = true;
	bool m_showBarChart  = true;
	int  m_graphOffset   = 0;

	int  m_cachedBattleCount = -1;
	int  m_cachedGraphOffset = -1;
	std::vector<float> m_gNums, m_gDps, m_gMelee, m_gDD, m_gDots, m_gPets;
	std::vector<float> m_gCrits, m_gHeals, m_gCritHeals;
	int  m_gStartIdx = 0;
	int  m_gCount    = 0;

	std::chrono::steady_clock::time_point m_lastFCTUpdate;
};
