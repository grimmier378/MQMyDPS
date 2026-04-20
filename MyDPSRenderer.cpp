#include "MyDPSRenderer.h"
#include "MQMyDPS.h"

#include <imgui/fonts/IconsMaterialDesign.h>
#include <imgui/implot/implot.h>
#include <fmt/format.h>

#include <algorithm>

static std::string FormatNumber(int64_t value)
{
	if (value >= 1000000)
		return fmt::format("{:.1f}m", value / 1000000.0);
	if (value >= 1000)
		return fmt::format("{:.1f}k", value / 1000.0);
	return fmt::format("{}", value);
}

void MyDPSRenderer::RenderCombatSpam(MyDPSEngine& engine)
{
	if (!engine.showCombatSpam || !engine.tracking)
		return;

	auto windowName = fmt::format("Combat Spam##{}", engine.charName);

	ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(engine.settings.bgColor.w);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoFocusOnAppearing;
	if (engine.settings.spamClickThrough)
		flags |= ImGuiWindowFlags_NoInputs;

	bool wasOpen = engine.showCombatSpam;
	if (ImGui::Begin(windowName.c_str(), &engine.showCombatSpam, flags))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * engine.settings.spamFontScale);

		auto now = std::chrono::steady_clock::now();

		for (auto it = engine.recentRecords.rbegin(); it != engine.recentRecords.rend(); ++it)
		{
			const auto& rec = *it;
			auto age = std::chrono::duration_cast<std::chrono::seconds>(now - rec.timestamp).count();
			if (age > engine.settings.displayTime)
				continue;

			const char* colorKey;
			if (rec.type == DamageType::Melee || rec.type == DamageType::PetMelee)
				colorKey = rec.attackVerb.c_str();
			else
				colorKey = DamageTypeToColorKey(rec.type);

			ImVec4 color(1, 1, 1, 1);
			auto colorIt = engine.settings.damageColors.find(colorKey);
			if (colorIt != engine.settings.damageColors.end())
				color = colorIt->second;

			std::string text;
			if (engine.settings.showType && engine.settings.showTarget)
				text = fmt::format("[{}] {}: {}", rec.attackVerb, rec.targetName, rec.isMiss ? "MISS" : FormatNumber(rec.damage));
			else if (engine.settings.showType)
				text = fmt::format("[{}] {}", rec.attackVerb, rec.isMiss ? "MISS" : FormatNumber(rec.damage));
			else if (engine.settings.showTarget)
				text = fmt::format("{}: {}", rec.targetName, rec.isMiss ? "MISS" : FormatNumber(rec.damage));
			else
				text = rec.isMiss ? "MISS" : FormatNumber(rec.damage);

			ImGui::TextColored(color, "%s", text.c_str());
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (wasOpen && !engine.showCombatSpam)
		engine.SaveCharacterSettings();
}

void MyDPSRenderer::RenderMainWindow(MyDPSEngine& engine)
{
	if (!engine.showMainWindow)
		return;

	auto windowName = fmt::format("DPS Report##{}", engine.charName);

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

	bool wasOpen = engine.showMainWindow;
	if (ImGui::Begin(windowName.c_str(), &engine.showMainWindow, ImGuiWindowFlags_MenuBar))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * engine.settings.fontScale);

		if (ImGui::BeginMenuBar())
		{
			if (engine.tracking)
			{
				if (ImGui::MenuItem(ICON_MD_STOP " Stop"))
					engine.tracking = false;
			}
			else
			{
				if (ImGui::MenuItem(ICON_MD_PLAY_ARROW " Start"))
				{
					engine.tracking = true;
					if (engine.sessionDamage == 0)
						engine.sessionStartTime = std::chrono::steady_clock::now();
				}
			}

			if (ImGui::MenuItem(ICON_MD_DELETE " Reset"))
				engine.ResetAll();

			if (ImGui::MenuItem(ICON_MD_SETTINGS " Config"))
				engine.showConfigWindow = !engine.showConfigWindow;

			ImGui::Separator();

			auto dpsIt = engine.settings.damageColors.find("dps");
			ImVec4 dpsColor = dpsIt != engine.settings.damageColors.end() ? dpsIt->second : ImVec4(1, 0.9f, 0.4f, 1);
			ImGui::TextColored(dpsColor, "DPS: %.1f", engine.GetSessionDPS());

			if (engine.inCombat)
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[COMBAT]");

			ImGui::EndMenuBar();
		}

		if (ImGui::BeginTabBar("DPSTabs"))
		{
			if (ImGui::BeginTabItem("Current Battle"))
			{
				RenderCurrentBattle(engine);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("History"))
			{
				RenderHistory(engine);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Targets"))
			{
				RenderTargets(engine);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Healing"))
			{
				RenderHealing(engine);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Graphs"))
			{
				RenderGraphs(engine);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopFont();
	}
	ImGui::End();

	if (wasOpen && !engine.showMainWindow)
		engine.SaveCharacterSettings();
}

void MyDPSRenderer::RenderCurrentBattle(MyDPSEngine& engine)
{
	if (!engine.inCombat && engine.battleDamage == 0)
	{
		ImGui::TextDisabled("No active battle. Attack something to begin.");
		return;
	}

	float battleDPS = engine.GetBattleDPS();
	float battleDur = engine.GetBattleDuration();
	float avgDmg = engine.battleHitCount > 0
		? static_cast<float>(engine.battleDamage) / static_cast<float>(engine.battleHitCount)
		: 0.0f;

	auto C = [&](const char* key) -> ImVec4 {
		auto it = engine.settings.damageColors.find(key);
		return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
	};

	if (engine.inCombat)
		ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Status: In Combat");
	else
		ImGui::TextColored(C("duration"), "Status: Combat Ending...");
	ImGui::Separator();

	if (ImGui::BeginTable("BattleStats", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Value");

		auto Row = [](const char* label, const std::string& val, const ImVec4& color) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", label);
			ImGui::TableNextColumn(); ImGui::TextColored(color, "%s", val.c_str());
		};

		Row("DPS",         fmt::format("{:.1f}", battleDPS),              C("dps"));
		Row("Duration",    fmt::format("{:.0f}s", battleDur),             C("duration"));
		Row("Total Damage",FormatNumber(engine.battleDamage),             C("total"));
		Row("Avg Damage",  fmt::format("{:.1f}", avgDmg),                C("avg"));
		Row("Hits",        fmt::format("{}", engine.battleHitCount),      C("total"));
		Row("Crit Damage", FormatNumber(engine.battleCritDamage),         C("crit"));
		Row("DoT Damage",  FormatNumber(engine.battleDotDamage),          C("dot"));
		Row("Pet Damage",  FormatNumber(engine.battlePetDamage),          C("pet"));
		Row("Non-Melee",   FormatNumber(engine.battleNonMeleeDmg),        C("non-melee"));
		Row("Heals",       FormatNumber(engine.battleDirectHeals),        C("heal"));
		Row("Crit Heals",  FormatNumber(engine.battleCritHeals),          C("critHeals"));

		ImGui::EndTable();
	}

	if (!engine.currentTargets.empty())
	{
		ImGui::Separator();
		ImGui::Text("Targets this battle:");

		if (ImGui::BeginTable("BattleTargets", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("DPS");
			ImGui::TableSetupColumn("Hits");
			ImGui::TableSetupColumn("Total");
			ImGui::TableHeadersRow();

			for (const auto& [id, data] : engine.currentTargets)
			{
				std::string label = data.spawnID > 0
					? fmt::format("{} (#{})", data.name, data.spawnID)
					: data.name;

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextColored(C("dmg-target"), "%s", label.c_str());
				ImGui::TableNextColumn(); ImGui::TextColored(C("dps"),        "%.1f", data.GetDPS());
				ImGui::TableNextColumn(); ImGui::TextColored(C("total"),      "%d", data.hitCount);
				ImGui::TableNextColumn(); ImGui::TextColored(C("total"),      "%s", FormatNumber(data.totalDamage).c_str());
			}

			ImGui::EndTable();
		}
	}

	if (!engine.currentHealTargets.empty())
	{
		ImGui::Separator();
		ImGui::Text("Healed Players:");

		if (ImGui::BeginTable("BattleHeals", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Player");
			ImGui::TableSetupColumn("Heals");
			ImGui::TableSetupColumn("Total");
			ImGui::TableHeadersRow();

			for (const auto& [name, ht] : engine.currentHealTargets)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextColored(C("heal-target"), "%s", ht.name.c_str());
				ImGui::TableNextColumn(); ImGui::TextColored(C("total"),       "%d", ht.healCount);
				ImGui::TableNextColumn(); ImGui::TextColored(C("heal"),        "%s", FormatNumber(ht.GetTotalHeals()).c_str());
			}

			ImGui::EndTable();
		}
	}
}

void MyDPSRenderer::RenderHistory(MyDPSEngine& engine)
{
	if (engine.battleHistory.empty())
	{
		ImGui::TextDisabled("No battles recorded yet.");
		return;
	}

	if (ImGui::BeginTable("History", 12,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable
		| ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Battle#", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Duration");
		ImGui::TableSetupColumn("Avg");
		ImGui::TableSetupColumn("Crits");
		ImGui::TableSetupColumn("DoTs");
		ImGui::TableSetupColumn("Pet");
		ImGui::TableSetupColumn("Non-Melee");
		ImGui::TableSetupColumn("Heals");
		ImGui::TableSetupColumn("Crit Heals");
		ImGui::TableSetupColumn("Total");
		ImGui::TableSetupColumn("Total Heals");
		ImGui::TableHeadersRow();

		int startIdx = engine.settings.sortNewest
			? static_cast<int>(engine.battleHistory.size()) - 1 : 0;
		int endIdx = engine.settings.sortNewest
			? -1 : static_cast<int>(engine.battleHistory.size());
		int step = engine.settings.sortNewest ? -1 : 1;

		for (int i = startIdx; i != endIdx; i += step)
		{
			const auto& b = engine.battleHistory[i];
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			bool expanded = ImGui::TreeNode("##battle", "Battle (%d) NPC (%d) Healed (%d)",
				b.battleNumber, static_cast<int>(b.targets.size()), static_cast<int>(b.healTargets.size()));
			auto Clr = [&](const char* key) -> ImVec4 {
				auto it = engine.settings.damageColors.find(key);
				return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
			};

			ImGui::TableNextColumn(); ImGui::TextColored(Clr("dps"),       "%.1f", b.dps);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("duration"),  "%.0fs", b.durationSeconds);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("avg"),       "%.0f", b.avgDamage);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("crit"),      "%s", FormatNumber(b.critDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("dot"),       "%s", FormatNumber(b.dotDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("pet"),       "%s", FormatNumber(b.petDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("non-melee"), "%s", FormatNumber(b.nonMeleeDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("heal"),      "%s", FormatNumber(b.directHeals).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("critHeals"), "%s", FormatNumber(b.critHeals).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("total"),     "%s", FormatNumber(b.totalDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("heal"),      "%s", FormatNumber(b.directHeals + b.critHeals).c_str());

			if (expanded)
			{
				auto C = [&](const char* key) -> ImVec4 {
					auto it = engine.settings.damageColors.find(key);
					return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
				};

				ImVec4 cDmgTgt  = C("dmg-target");
				ImVec4 cHealTgt = C("heal-target");
				ImVec4 cDPS     = C("dps");
				ImVec4 cDur     = C("duration");
				ImVec4 cAvg     = C("avg");
				ImVec4 cCrit    = C("crit");
				ImVec4 cDoT     = C("dot");
				ImVec4 cPet     = C("pet");
				ImVec4 cNonMel  = C("non-melee");
				ImVec4 cHeal    = C("heal");
				ImVec4 cCritH   = C("critHeals");
				ImVec4 cTotal   = C("total");

				for (const auto& [tgtID, tgt] : b.targets)
				{
					std::string tgtLabel = tgt.spawnID > 0
						? fmt::format("  {} (#{})", tgt.name, tgt.spawnID)
						: fmt::format("  {}", tgt.name);
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextColored(cDmgTgt, "%s", tgtLabel.c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cDPS,    "%.1f", tgt.GetDPS());
					ImGui::TableNextColumn(); ImGui::TextColored(cDur,    "%.0fs", tgt.GetDurationSeconds());
					ImGui::TableNextColumn(); ImGui::TextColored(cAvg,    "%.0f", tgt.GetAvgDamage());
					ImGui::TableNextColumn(); ImGui::TextColored(cCrit,   "%s", FormatNumber(tgt.critDamage).c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cDoT,    "%s", FormatNumber(tgt.dotDamage).c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cPet,    "%s", FormatNumber(tgt.petDamage).c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cNonMel, "%s", FormatNumber(tgt.nonMeleeDamage).c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cDur,    "-");
					ImGui::TableNextColumn(); ImGui::TextColored(cDur,    "-");
					ImGui::TableNextColumn(); ImGui::TextColored(cTotal,  "%s", FormatNumber(tgt.totalDamage).c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cDur,    "-");
				}

				if (!b.healTargets.empty())
				{
					for (const auto& [htName, ht] : b.healTargets)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn(); ImGui::TextColored(cHealTgt, "  %s", ht.name.c_str());
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cHeal,    "%s", FormatNumber(ht.directHeals).c_str());
						ImGui::TableNextColumn(); ImGui::TextColored(cCritH,   "%s", FormatNumber(ht.critHeals).c_str());
						ImGui::TableNextColumn(); ImGui::TextColored(cDur,     "-");
						ImGui::TableNextColumn(); ImGui::TextColored(cTotal,   "%s", FormatNumber(ht.GetTotalHeals()).c_str());
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void MyDPSRenderer::RenderTargets(MyDPSEngine& engine)
{
	if (engine.currentTargets.empty() && engine.battleHistory.empty())
	{
		ImGui::TextDisabled("No target data yet.");
		return;
	}

	if (!engine.currentTargets.empty())
	{
		ImGui::Text("Current Battle Targets:");

		if (ImGui::BeginTable("CurTargets", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("DPS");
			ImGui::TableSetupColumn("Hits");
			ImGui::TableSetupColumn("Avg");
			ImGui::TableSetupColumn("Total");
			ImGui::TableHeadersRow();

			for (const auto& [id, data] : engine.currentTargets)
			{
				std::string label = data.spawnID > 0
					? fmt::format("{} (#{})", data.name, data.spawnID)
					: data.name;

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", label.c_str());
				ImGui::TableNextColumn(); ImGui::Text("%.1f", data.GetDPS());
				ImGui::TableNextColumn(); ImGui::Text("%d", data.hitCount);
				ImGui::TableNextColumn(); ImGui::Text("%.0f", data.GetAvgDamage());
				ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(data.totalDamage).c_str());
			}

			ImGui::EndTable();
		}
	}

	if (!engine.battleHistory.empty())
	{
		ImGui::Separator();
		ImGui::Text("All Session Targets:");

		std::unordered_map<int, TargetDamageData> allTargets;
		for (const auto& battle : engine.battleHistory)
		{
			for (const auto& [id, data] : battle.targets)
			{
				auto& agg = allTargets[id];
				if (agg.name.empty())
				{
					agg.name = data.name;
					agg.spawnID = data.spawnID;
				}
				agg.totalDamage += data.totalDamage;
				agg.critDamage += data.critDamage;
				agg.dotDamage += data.dotDamage;
				agg.dsDamage += data.dsDamage;
				agg.petDamage += data.petDamage;
				agg.nonMeleeDamage += data.nonMeleeDamage;
				agg.hitCount += data.hitCount;
				agg.missCount += data.missCount;
			}
		}

		for (const auto& [id, data] : engine.currentTargets)
		{
			auto& agg = allTargets[id];
			if (agg.name.empty())
			{
				agg.name = data.name;
				agg.spawnID = data.spawnID;
			}
			agg.totalDamage += data.totalDamage;
			agg.critDamage += data.critDamage;
			agg.dotDamage += data.dotDamage;
			agg.dsDamage += data.dsDamage;
			agg.petDamage += data.petDamage;
			agg.nonMeleeDamage += data.nonMeleeDamage;
			agg.hitCount += data.hitCount;
			agg.missCount += data.missCount;
		}

		struct AggEntry { int id; const TargetDamageData* data; };
		std::vector<AggEntry> sorted;
		sorted.reserve(allTargets.size());
		for (const auto& [id, data] : allTargets)
			sorted.push_back({ id, &data });
		std::sort(sorted.begin(), sorted.end(), [](const AggEntry& a, const AggEntry& b) {
			return a.data->totalDamage > b.data->totalDamage;
		});

		if (ImGui::BeginTable("AllTargets", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Hits");
			ImGui::TableSetupColumn("Avg");
			ImGui::TableSetupColumn("Total");
			ImGui::TableHeadersRow();

			for (const auto& entry : sorted)
			{
				std::string label = entry.data->spawnID > 0
					? fmt::format("{} (#{})", entry.data->name, entry.data->spawnID)
					: entry.data->name;

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", label.c_str());
				ImGui::TableNextColumn(); ImGui::Text("%d", entry.data->hitCount);
				ImGui::TableNextColumn(); ImGui::Text("%.0f", entry.data->GetAvgDamage());
				ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(entry.data->totalDamage).c_str());
			}

			ImGui::EndTable();
		}
	}
}

void MyDPSRenderer::RenderGraphs(MyDPSEngine& engine)
{
	if (engine.battleHistory.empty() && !engine.inCombat)
	{
		ImGui::TextDisabled("No data to graph yet.");
		return;
	}

	ImGui::Checkbox("Line Graph", &m_showLineGraph);
	ImGui::SameLine();
	ImGui::Checkbox("Bar Chart", &m_showBarChart);
	ImGui::SameLine();
	ImGui::Checkbox("Pie Chart", &m_showPieChart);

	if (m_showLineGraph || m_showBarChart)
		RenderLineGraph(engine);

	if (m_showPieChart)
		RenderPieChart(engine);
}

static constexpr int GRAPH_WINDOW   = 50;
static constexpr int GRAPH_MAX_BACK = 250;

void MyDPSRenderer::RenderLineGraph(MyDPSEngine& engine)
{
	int totalBattles = static_cast<int>(engine.battleHistory.size());
	if (totalBattles == 0)
	{
		ImGui::TextDisabled("No completed battles to graph.");
		return;
	}

	int maxBack = std::min(totalBattles, GRAPH_MAX_BACK);
	int maxOffset = std::max(0, maxBack - GRAPH_WINDOW);
	m_graphOffset = std::clamp(m_graphOffset, 0, maxOffset);

	int startIdx = totalBattles - maxBack + m_graphOffset;
	int count = std::min(GRAPH_WINDOW, maxBack - m_graphOffset);
	if (startIdx < 0) startIdx = 0;
	if (startIdx + count > totalBattles) count = totalBattles - startIdx;

	if (maxBack > GRAPH_WINDOW)
	{
		ImGui::SetNextItemWidth(-1);
		ImGui::SliderInt("##GraphScroll", &m_graphOffset, 0, maxOffset,
			fmt::format("Battles {}-{} of {}", startIdx + 1, startIdx + count, totalBattles).c_str());
	}

	auto C = [&](const char* key) -> ImVec4 {
		auto it = engine.settings.damageColors.find(key);
		return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
	};

	std::vector<float> nums(count), dps(count);
	std::vector<float> melee(count), dd(count), dots(count), pets(count);
	std::vector<float> crits(count), heals(count), critHeals(count);

	for (int i = 0; i < count; i++)
	{
		const auto& b = engine.battleHistory[startIdx + i];
		nums[i]      = static_cast<float>(b.battleNumber);
		dps[i]       = b.dps;
		crits[i]     = static_cast<float>(b.critDamage);
		dots[i]      = static_cast<float>(b.dotDamage);
		pets[i]      = static_cast<float>(b.petDamage);
		dd[i]        = static_cast<float>(b.nonMeleeDamage);
		heals[i]     = static_cast<float>(b.directHeals);
		critHeals[i] = static_cast<float>(b.critHeals);
		melee[i]     = static_cast<float>(b.totalDamage) - crits[i] - dots[i] - dd[i] - pets[i];
		if (melee[i] < 0) melee[i] = 0;
	}

	float xMin = count > 0 ? nums[0] - 0.5f : 0;
	float xMax = count > 0 ? nums[count - 1] + 1.5f : 1;

	if (m_showLineGraph)
	{
		if (ImPlot::BeginPlot("DPS by Battle", ImVec2(-1, 300)))
		{
			ImPlot::SetupAxes("Battle #", "DPS");
			ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, HUGE_VAL);

			ImPlot::SetNextLineStyle(C("dps"), 2.0f);
			ImPlot::PlotLine("DPS", nums.data(), dps.data(), count);

			ImPlot::EndPlot();
		}

		if (ImPlot::BeginPlot("Damage Breakdown by Battle", ImVec2(-1, 350)))
		{
			ImPlot::SetupAxes("Battle #", "Damage");
			ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, HUGE_VAL);

			ImPlot::SetupAxis(ImAxis_Y2, "Crits", ImPlotAxisFlags_AuxDefault);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y2, 0, HUGE_VAL);

			ImPlot::SetupAxis(ImAxis_Y3, "Heals", ImPlotAxisFlags_AuxDefault);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y3, 0, HUGE_VAL);

			ImPlot::SetupLegend(ImPlotLocation_NorthEast);

			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
			ImPlot::SetNextLineStyle(C("hit"), 2.0f);
			ImPlot::PlotLine("Melee", nums.data(), melee.data(), count);
			ImPlot::SetNextLineStyle(C("non-melee"), 1.5f);
			ImPlot::PlotLine("DD", nums.data(), dd.data(), count);
			ImPlot::SetNextLineStyle(C("dot"), 1.5f);
			ImPlot::PlotLine("DoT", nums.data(), dots.data(), count);
			ImPlot::SetNextLineStyle(C("pet"), 1.5f);
			ImPlot::PlotLine("Pet", nums.data(), pets.data(), count);

			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
			ImPlot::SetNextLineStyle(C("crit"), 1.5f);
			ImPlot::PlotLine("Crit", nums.data(), crits.data(), count);

			ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
			ImPlot::SetNextLineStyle(C("heal"), 1.5f);
			ImPlot::PlotLine("Heals", nums.data(), heals.data(), count);
			ImPlot::SetNextLineStyle(C("critHeals"), 1.5f);
			ImPlot::PlotLine("Crit Heals", nums.data(), critHeals.data(), count);

			ImPlot::EndPlot();
		}
	}

	if (m_showBarChart)
	{
		if (ImPlot::BeginPlot("Damage Breakdown Bars", ImVec2(-1, 300)))
		{
			ImPlot::SetupAxes("Battle #", "Damage");
			ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
			ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, HUGE_VAL);
			ImPlot::SetupLegend(ImPlotLocation_NorthEast);

			constexpr int numSeries = 7;
			constexpr float groupWidth = 0.8f;
			constexpr float barWidth = groupWidth / numSeries;
			float startOff = -groupWidth * 0.5f + barWidth * 0.5f;

			struct BarSeries { const char* label; const char* colorKey; std::vector<float>* data; };
			BarSeries series[] = {
				{ "Melee",      "hit",       &melee },
				{ "DD",         "non-melee", &dd },
				{ "DoT",        "dot",       &dots },
				{ "Pet",        "pet",       &pets },
				{ "Crit",       "crit",      &crits },
				{ "Heals",      "heal",      &heals },
				{ "Crit Heals", "critHeals", &critHeals },
			};

			for (int s = 0; s < numSeries; s++)
			{
				std::vector<float> shifted(count);
				for (int i = 0; i < count; i++)
					shifted[i] = nums[i] + startOff + s * barWidth;

				ImPlot::SetNextFillStyle(C(series[s].colorKey));
				ImPlot::PlotBars(series[s].label, shifted.data(), series[s].data->data(), count, barWidth * 0.9f);
			}

			ImPlot::EndPlot();
		}
	}
}

void MyDPSRenderer::RenderPieChart(MyDPSEngine& engine)
{
	std::unordered_map<std::string, int64_t> targetTotals;

	for (const auto& battle : engine.battleHistory)
		for (const auto& [id, data] : battle.targets)
			targetTotals[data.name] += data.totalDamage;

	for (const auto& [id, data] : engine.currentTargets)
		targetTotals[data.name] += data.totalDamage;

	if (targetTotals.empty())
	{
		ImGui::TextDisabled("No target data for pie chart.");
		return;
	}

	struct PieEntry { std::string name; double value; };
	std::vector<PieEntry> entries;
	entries.reserve(targetTotals.size());
	for (const auto& [name, total] : targetTotals)
		entries.push_back({ name, static_cast<double>(total) });
	std::sort(entries.begin(), entries.end(), [](const PieEntry& a, const PieEntry& b) {
		return a.value > b.value;
	});

	std::vector<const char*> labels;
	std::vector<double> values;
	labels.reserve(entries.size());
	values.reserve(entries.size());
	for (const auto& e : entries)
	{
		labels.push_back(e.name.c_str());
		values.push_back(e.value);
	}

	ImGui::Text("Damage Distribution by Target");

	if (ImPlot::BeginPlot("##DmgPie", ImVec2(-1, 350), ImPlotFlags_Equal))
	{
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
		ImPlot::PlotPieChart(labels.data(), values.data(), static_cast<int>(values.size()),
			0.5, 0.5, 0.4, "%.0f", 90.0);
		ImPlot::EndPlot();
	}
}

void MyDPSRenderer::RenderHealing(MyDPSEngine& engine)
{
	std::unordered_map<std::string, HealTargetData> allHeals;

	for (const auto& battle : engine.battleHistory)
	{
		for (const auto& [name, ht] : battle.healTargets)
		{
			auto& agg = allHeals[name];
			if (agg.name.empty())
				agg.name = ht.name;
			agg.directHeals += ht.directHeals;
			agg.critHeals += ht.critHeals;
			agg.healCount += ht.healCount;
		}
	}

	for (const auto& [name, ht] : engine.currentHealTargets)
	{
		auto& agg = allHeals[name];
		if (agg.name.empty())
			agg.name = ht.name;
		agg.directHeals += ht.directHeals;
		agg.critHeals += ht.critHeals;
		agg.healCount += ht.healCount;
	}

	if (allHeals.empty())
	{
		ImGui::TextDisabled("No healing data yet.");
		return;
	}

	struct HealEntry { std::string name; const HealTargetData* data; };
	std::vector<HealEntry> sorted;
	sorted.reserve(allHeals.size());
	for (const auto& [name, data] : allHeals)
		sorted.push_back({ name, &data });
	std::sort(sorted.begin(), sorted.end(), [](const HealEntry& a, const HealEntry& b) {
		return a.data->GetTotalHeals() > b.data->GetTotalHeals();
	});

	if (ImGui::BeginTable("HealTargets", 5,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable))
	{
		ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Direct Heals");
		ImGui::TableSetupColumn("Crit Heals");
		ImGui::TableSetupColumn("Count");
		ImGui::TableSetupColumn("Total");
		ImGui::TableHeadersRow();

		for (const auto& entry : sorted)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", entry.name.c_str());
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(entry.data->directHeals).c_str());
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(entry.data->critHeals).c_str());
			ImGui::TableNextColumn(); ImGui::Text("%d", entry.data->healCount);
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(entry.data->GetTotalHeals()).c_str());
		}

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("Healing Distribution by Player");

	std::vector<const char*> labels;
	std::vector<double> values;
	labels.reserve(sorted.size());
	values.reserve(sorted.size());
	for (const auto& e : sorted)
	{
		labels.push_back(e.name.c_str());
		values.push_back(static_cast<double>(e.data->GetTotalHeals()));
	}

	if (ImPlot::BeginPlot("##HealPie", ImVec2(-1, 350), ImPlotFlags_Equal))
	{
		ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
		ImPlot::PlotPieChart(labels.data(), values.data(), static_cast<int>(values.size()),
			0.5, 0.5, 0.4, "%.0f", 90.0);
		ImPlot::EndPlot();
	}
}

void MyDPSRenderer::RenderConfigWindow(MyDPSEngine& engine)
{
	if (!engine.showConfigWindow)
		return;

	auto windowName = fmt::format("DPS Config##{}", engine.charName);
	ImGui::SetNextWindowSize(ImVec2(350, 450), ImGuiCond_FirstUseEver);

	bool wasOpen = engine.showConfigWindow;
	if (ImGui::Begin(windowName.c_str(), &engine.showConfigWindow))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * engine.settings.fontScale);
		RenderConfig(engine);
		ImGui::PopFont();
	}

	ImGui::End();

	if (wasOpen && !engine.showConfigWindow)
		engine.SaveCharacterSettings();
}

void MyDPSRenderer::RenderConfig(MyDPSEngine& engine)
{
	auto& s = engine.settings;

	if (ImGui::Button("Save"))
	{
		engine.SaveCharacterSettings();
		engine.showConfigWindow = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
		engine.LoadCharacterSettings();

	ImGui::Separator();

	struct BoolEntry { const char* label; bool* value; };
	struct IntEntry  { const char* label; int* value; int min; int max; };
	struct FloatEntry{ const char* label; float* value; float min; float max; };

	BoolEntry bools[] = {
		{ "Show Type",         &s.showType },
		{ "Show Target",       &s.showTarget },
		{ "Show My Misses",    &s.showMyMisses },
		{ "Show Missed Me",    &s.showMissMe },
		{ "Show Hit Me",       &s.showHitMe },
		{ "Show Crit Heals",   &s.showCritHeals },
		{ "Show Damage Shield",&s.showDS },
		{ "Track Pet",         &s.addPet },
		{ "Show Combat Spam",  &engine.showCombatSpam },
		{ "Sort Newest First", &s.sortNewest },
		{ "Auto Start",        &s.autoStart },
		{ "Lock Spam Window",  &s.spamClickThrough },
	};

	IntEntry ints[] = {
		{ "Display Time (s)",     &s.displayTime,    1, 60 },
		{ "Battle End Delay (s)", &s.battleEndDelay,  1, 30 },
	};

	FloatEntry floats[] = {
		{ "Font Scale",      &s.fontScale,     0.5f, 2.0f },
		{ "Spam Font Scale", &s.spamFontScale, 0.5f, 2.0f },
	};

	int sizeX = static_cast<int>(ImGui::GetWindowWidth());

	if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int col = std::max(1, sizeX / 165);
		if (ImGui::BeginTable("Display Toggles", col))
		{
			ImGui::TableNextRow();
			for (auto& [label, val] : bools)
			{
				ImGui::TableNextColumn();
				ImGui::Checkbox(label, val);
			}
			ImGui::EndTable();
		}
	}
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int col = std::max(1, sizeX / 220);
		if (ImGui::BeginTable("Timing Sliders", col))
		{
			ImGui::TableNextRow();
			for (auto& [label, val, lo, hi] : ints)
			{
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(100);
				ImGui::SliderInt(label, val, lo, hi, "%d", ImGuiSliderFlags_AlwaysClamp);
			}
			ImGui::EndTable();
		}
	}
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Scale", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int col = std::max(1, sizeX / 220);
		if (ImGui::BeginTable("Font Scaling", col))
		{
			ImGui::TableNextRow();
			for (auto& [label, val, lo, hi] : floats)
			{
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat(label, val, lo, hi, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			}
			ImGui::EndTable();
		}
	}
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit4("Background", &s.bgColor.x, ImGuiColorEditFlags_NoInputs);

		int col = std::max(1, sizeX / 150);
		if (ImGui::BeginTable("Damage Colors", col))
		{
			ImGui::TableNextRow();
			for (auto& [name, color] : s.damageColors)
			{
				ImGui::TableNextColumn();
				ImGui::ColorEdit4(name.c_str(), &color.x, ImGuiColorEditFlags_NoInputs);
			}
			ImGui::EndTable();
		}
	}
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Floating Combat Text"))
	{
		ImGui::Checkbox("Enable FCT", &s.showFCT);

		BoolEntry fctBools[] = {
			{ "Melee",      &s.showFCT_Melee },
			{ "DD",         &s.showFCT_DD },
			{ "DoT",        &s.showFCT_DoT },
			{ "Pet",        &s.showFCT_Pet },
			{ "Crit",       &s.showFCT_Crit },
			{ "Heals",      &s.showFCT_Heals },
			{ "Crit Heals", &s.showFCT_CritHeals },
			{ "Hit By",     &s.showFCT_HitBy },
		};

		int fctCol = std::max(1, sizeX / 120);
		if (ImGui::BeginTable("FCT Toggles", fctCol))
		{
			ImGui::TableNextRow();
			for (auto& [label, val] : fctBools)
			{
				ImGui::TableNextColumn();
				ImGui::Checkbox(label, val);
			}
			ImGui::EndTable();
		}

		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("Base Font Size", &s.fctBaseFontSize, 12.0f, 48.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("Font Scale", &s.fctFontScale, 0.5f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("Shadow Offset", &s.fctShadowOffset, 0.0f, 5.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("Lifetime (s)", &s.fctLifetime, 1.0f, 5.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
	}
}

void MyDPSRenderer::RenderFloatingText(MyDPSEngine& engine)
{
	if (!engine.settings.showFCT)
		return;

	auto now = std::chrono::steady_clock::now();
	float dt = std::chrono::duration<float>(now - m_lastFCTUpdate).count();
	m_lastFCTUpdate = now;

	if (dt <= 0.0f || dt > 0.1f)
		dt = 0.016f;

	engine.fctManager.Update(dt);
	engine.fctManager.Render(engine.settings);
}
