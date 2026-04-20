#pragma once

#include <chrono>

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

	std::chrono::steady_clock::time_point m_lastFCTUpdate;
};
