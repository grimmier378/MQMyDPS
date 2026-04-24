#include "MyDPSRenderer.h"
#include "MQMyDPS.h"
#include "Theme.h"

#include <mq/Plugin.h>
#include <mq/imgui/Widgets.h>
#include <imgui/fonts/IconsMaterialDesign.h>
#include <imgui/implot/implot.h>
#include <fmt/format.h>

#include <algorithm>

MyDPSRenderer::~MyDPSRenderer()
{
	delete m_pPickerSpellAnim;
	m_pPickerSpellAnim = nullptr;
	delete m_pPickerItemAnim;
	m_pPickerItemAnim = nullptr;
}

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

	auto oldStyle = ImGuiTheme::ApplyTheme(engine.settings.themeIdx);

	auto windowName = fmt::format("Combat Output##{}", engine.charName);

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
			if (rec.type == DamageType::PetMelee || rec.type == DamageType::PetNonMelee)
				colorKey = "pet";
			else if (rec.type == DamageType::Melee)
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
	ImGuiTheme::ResetTheme(oldStyle);

	if (wasOpen && !engine.showCombatSpam)
		engine.SaveCharacterSettings();
}

void MyDPSRenderer::RenderMainWindow(MyDPSEngine& engine)
{
	if (!engine.showMainWindow)
		return;

	auto oldStyle = ImGuiTheme::ApplyTheme(engine.settings.themeIdx);

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
				if (ImGui::BeginChild("##BattleChild", ImVec2(0, 0), ImGuiChildFlags_None))
					RenderCurrentBattle(engine);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("History"))
			{
				if (ImGui::BeginChild("##HistoryChild", ImVec2(0, 0), ImGuiChildFlags_None))
					RenderHistory(engine);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Targets"))
			{
				if (ImGui::BeginChild("##TargetsChild", ImVec2(0, 0), ImGuiChildFlags_None))
					RenderTargets(engine);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Healing"))
			{
				if (ImGui::BeginChild("##HealingChild", ImVec2(0, 0), ImGuiChildFlags_None))
					RenderHealing(engine);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Graphs"))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, ImGui::GetStyle().WindowPadding.y));
				if (ImGui::BeginChild("##GraphsChild", ImVec2(0, 0), ImGuiChildFlags_Borders))
					RenderGraphs(engine);
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::PopFont();
	}
	ImGui::End();
	ImGuiTheme::ResetTheme(oldStyle);

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

	struct StatEntry { const char* label; std::string value; ImVec4 color; };
	StatEntry stats[] = {
		{ "DPS",        fmt::format("{:.1f}", battleDPS),         C("dps") },
		{ "Duration",   fmt::format("{:.0f} s", battleDur),       C("duration") },
		{ "Total",      FormatNumber(engine.battleDamage),        C("total") },
		{ "Avg",        fmt::format("{:.1f}", avgDmg),            C("avg") },
		{ "Hits",       fmt::format("{}", engine.battleHitCount), C("total") },
		{ "Crits",      FormatNumber(engine.battleCritDamage),    C("crit") },
		{ "DoTs",       FormatNumber(engine.battleDotDamage),     C("dot") },
		{ "Pet",        FormatNumber(engine.battlePetDamage),     C("pet") },
		{ "Non-Melee",  FormatNumber(engine.battleNonMeleeDmg),   C("non-melee") },
		{ "Heals",      FormatNumber(engine.battleDirectHeals),   C("heal") },
		{ "Crit Heals", FormatNumber(engine.battleCritHeals),     C("critHeals") },
	};

	int sizeX = static_cast<int>(ImGui::GetWindowWidth());
	int col = std::max(1, sizeX / 150);
	if (ImGui::BeginTable("BattleStats", col))
	{
		ImGui::TableNextRow();
		for (const auto& s : stats)
		{
			ImGui::TableNextColumn();
			ImGui::Text("%s:", s.label);
			ImGui::SameLine();
			ImGui::TextColored(s.color, "%s", s.value.c_str());
		}
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

	if (ImGui::BeginTable("History", 13,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable
		| ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Battle#", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Duration");
		ImGui::TableSetupColumn("Avg");
		ImGui::TableSetupColumn("Melee");
		ImGui::TableSetupColumn("Crits");
		ImGui::TableSetupColumn("DoTs");
		ImGui::TableSetupColumn("Pet");
		ImGui::TableSetupColumn("Non-Melee");
		ImGui::TableSetupColumn("Heals");
		ImGui::TableSetupColumn("Crit Heals");
		ImGui::TableSetupColumn("Total Damage");
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

			int64_t bMelee = b.totalDamage - b.nonMeleeDamage - b.dotDamage - b.petDamage - b.dsDamage;
			if (bMelee < 0) bMelee = 0;

			ImGui::TableNextColumn(); ImGui::TextColored(Clr("dps"),       "%.1f", b.dps);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("duration"),  "%.0fs", b.durationSeconds);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("avg"),       "%.0f", b.avgDamage);
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("hit"),       "%s", FormatNumber(bMelee).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("crit"),      "%s", FormatNumber(b.critDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("dot"),       "%s", FormatNumber(b.dotDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("pet"),       "%s", FormatNumber(b.petDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("non-melee"), "%s", FormatNumber(b.nonMeleeDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("heal"),      "%s", FormatNumber(b.directHeals).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("critHeals"), "%s", FormatNumber(b.critHeals).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("total"),     "%s", FormatNumber(b.totalDamage).c_str());
			ImGui::TableNextColumn(); ImGui::TextColored(Clr("heal"),      "%s", FormatNumber(b.directHeals).c_str());

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
				ImVec4 cHit     = C("hit");
				ImVec4 cCrit    = C("crit");
				ImVec4 cDoT     = C("dot");
				ImVec4 cPet     = C("pet");
				ImVec4 cNonMel  = C("non-melee");
				ImVec4 cHeal    = C("heal");
				ImVec4 cCritH   = C("critHeals");
				ImVec4 cTotal   = C("total");

				for (const auto& [tgtID, tgt] : b.targets)
				{
					int64_t tMelee = tgt.totalDamage - tgt.nonMeleeDamage - tgt.dotDamage - tgt.petDamage - tgt.dsDamage;
					if (tMelee < 0) tMelee = 0;

					std::string tgtLabel = tgt.spawnID > 0
						? fmt::format("  {} (#{})", tgt.name, tgt.spawnID)
						: fmt::format("  {}", tgt.name);
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextColored(cDmgTgt, "%s", tgtLabel.c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(cDPS,    "%.1f", tgt.GetDPS());
					ImGui::TableNextColumn(); ImGui::TextColored(cDur,    "%.0fs", tgt.GetDurationSeconds());
					ImGui::TableNextColumn(); ImGui::TextColored(cAvg,    "%.0f", tgt.GetAvgDamage());
					ImGui::TableNextColumn(); ImGui::TextColored(cHit,    "%s", FormatNumber(tMelee).c_str());
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

	if (!engine.sortedSessionTargets.empty())
	{
		ImGui::Separator();
		ImGui::Text("All Session Targets:");

		if (ImGui::BeginTable("AllTargets", 4,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Hits");
			ImGui::TableSetupColumn("Avg");
			ImGui::TableSetupColumn("Total");
			ImGui::TableHeadersRow();

			for (const auto& data : engine.sortedSessionTargets)
			{
				std::string label = data.spawnID > 0
					? fmt::format("{} (#{})", data.name, data.spawnID)
					: data.name;

				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", label.c_str());
				ImGui::TableNextColumn(); ImGui::Text("%d", data.hitCount);
				ImGui::TableNextColumn(); ImGui::Text("%.0f", data.GetAvgDamage());
				ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(data.totalDamage).c_str());
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

	ImGui::Checkbox("DPS Graph", &m_showDpsGraph);
	ImGui::SameLine();
	ImGui::Checkbox("Damage Breakdown", &m_showDmgGraph);
	ImGui::SameLine();
	ImGui::Checkbox("Bar Chart", &m_showBarChart);
	ImGui::SameLine();
	ImGui::Checkbox("Pie Chart", &m_showPieChart);

	RebuildGraphCache(engine);

	if (m_showDpsGraph)
		RenderDPSGraph(engine);

	if (m_showDmgGraph)
		RenderDamageGraph(engine);

	if (m_showBarChart)
		RenderBarChart(engine);

	if (m_showPieChart)
		RenderPieChart(engine);
}

static constexpr int GRAPH_WINDOW   = 50;
static constexpr int GRAPH_MAX_BACK = 250;

void MyDPSRenderer::RebuildGraphCache(MyDPSEngine& engine)
{
	int totalBattles = static_cast<int>(engine.battleHistory.size());
	if (totalBattles == m_cachedBattleCount)
		return;

	m_cachedBattleCount = totalBattles;
	int cacheCount = std::min(totalBattles, GRAPH_MAX_BACK);
	int cacheStart = totalBattles - cacheCount;
	m_gTotalCached = cacheCount;

	auto resize = [&](std::vector<float>& v) { v.resize(cacheCount); };
	resize(m_gNums); resize(m_gDps); resize(m_gMelee); resize(m_gDD);
	resize(m_gDots); resize(m_gPets); resize(m_gCrits); resize(m_gHeals);
	resize(m_gCritHeals);

	for (int i = 0; i < cacheCount; i++)
	{
		const auto& b = engine.battleHistory[cacheStart + i];
		m_gNums[i]      = static_cast<float>(b.battleNumber);
		m_gDps[i]       = b.dps;
		m_gCrits[i]     = static_cast<float>(b.critDamage);
		m_gDots[i]      = static_cast<float>(b.dotDamage);
		m_gPets[i]      = static_cast<float>(b.petDamage);
		m_gDD[i]        = static_cast<float>(b.nonMeleeDamage);
		m_gHeals[i]     = static_cast<float>(b.directHeals);
		m_gCritHeals[i] = static_cast<float>(b.critHeals);
		m_gMelee[i]     = static_cast<float>(b.totalDamage) - m_gDots[i] - m_gDD[i] - m_gPets[i] - static_cast<float>(b.dsDamage);
		if (m_gMelee[i] < 0) m_gMelee[i] = 0;
	}
}

void MyDPSRenderer::RenderScrollBar(const char* id, MyDPSRenderer::GraphScrollState& scroll, int totalBattles, int maxBack)
{
	int maxOffset = std::max(0, maxBack - GRAPH_WINDOW);
	int prev = scroll.offset;

	if (scroll.autoScroll)
		scroll.offset = maxOffset;

	scroll.offset = std::clamp(scroll.offset, 0, maxOffset);

	if (maxBack > GRAPH_WINDOW)
	{
		int startIdx = totalBattles - maxBack + scroll.offset;
		int count = std::min(GRAPH_WINDOW, maxBack - scroll.offset);

		ImGui::SetNextItemWidth(-1);
		ImGui::SliderInt(id, &scroll.offset, 0, maxOffset,
			fmt::format("Battles {}-{} of {}", startIdx + 1, startIdx + count, totalBattles).c_str());

		if (scroll.offset != prev)
			scroll.autoScroll = (scroll.offset >= maxOffset);
	}
}

void MyDPSRenderer::RenderDPSGraph(MyDPSEngine& engine)
{
	int totalBattles = static_cast<int>(engine.battleHistory.size());
	if (totalBattles == 0)
		return;

	int maxBack = std::min(totalBattles, GRAPH_MAX_BACK);
	RenderScrollBar("##DPSScroll", m_dpsScroll, totalBattles, maxBack);

	int startIdx = m_dpsScroll.offset;
	int count = std::min(GRAPH_WINDOW, m_gTotalCached - m_dpsScroll.offset);
	if (count <= 0)
		return;

	float xMin = m_gNums[startIdx] - 0.5f;
	float xMax = m_gNums[startIdx + count - 1] + 1.5f;

	auto C = [&](const char* key) -> ImVec4 {
		auto it = engine.settings.damageColors.find(key);
		return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
	};

	if (ImPlot::BeginPlot("DPS by Battle", ImVec2(-1, 300)))
	{
		ImPlot::SetupAxes("Battle #", "DPS");
		ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
		ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, HUGE_VAL);

		ImPlot::SetNextLineStyle(C("dps"), 2.0f);
		ImPlot::PlotLine("DPS", m_gNums.data() + startIdx, m_gDps.data() + startIdx, count);

		ImPlot::EndPlot();
	}
}

void MyDPSRenderer::RenderDamageGraph(MyDPSEngine& engine)
{
	int totalBattles = static_cast<int>(engine.battleHistory.size());
	if (totalBattles == 0)
		return;

	int maxBack = std::min(totalBattles, GRAPH_MAX_BACK);
	RenderScrollBar("##DmgScroll", m_dmgScroll, totalBattles, maxBack);

	int startIdx = m_dmgScroll.offset;
	int count = std::min(GRAPH_WINDOW, m_gTotalCached - m_dmgScroll.offset);
	if (count <= 0)
		return;

	float xMin = m_gNums[startIdx] - 0.5f;
	float xMax = m_gNums[startIdx + count - 1] + 1.5f;

	auto C = [&](const char* key) -> ImVec4 {
		auto it = engine.settings.damageColors.find(key);
		return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
	};

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
		ImPlot::PlotLine("Melee", m_gNums.data() + startIdx, m_gMelee.data() + startIdx, count);
		ImPlot::SetNextLineStyle(C("non-melee"), 1.5f);
		ImPlot::PlotLine("DD", m_gNums.data() + startIdx, m_gDD.data() + startIdx, count);
		ImPlot::SetNextLineStyle(C("dot"), 1.5f);
		ImPlot::PlotLine("DoT", m_gNums.data() + startIdx, m_gDots.data() + startIdx, count);
		ImPlot::SetNextLineStyle(C("pet"), 1.5f);
		ImPlot::PlotLine("Pet", m_gNums.data() + startIdx, m_gPets.data() + startIdx, count);

		ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
		ImPlot::SetNextLineStyle(C("crit"), 1.5f);
		ImPlot::PlotLine("Crit", m_gNums.data() + startIdx, m_gCrits.data() + startIdx, count);

		ImPlot::SetAxes(ImAxis_X1, ImAxis_Y3);
		ImPlot::SetNextLineStyle(C("heal"), 1.5f);
		ImPlot::PlotLine("Heals", m_gNums.data() + startIdx, m_gHeals.data() + startIdx, count);
		ImPlot::SetNextLineStyle(C("critHeals"), 1.5f);
		ImPlot::PlotLine("Crit Heals", m_gNums.data() + startIdx, m_gCritHeals.data() + startIdx, count);

		ImPlot::EndPlot();
	}
}

void MyDPSRenderer::RenderBarChart(MyDPSEngine& engine)
{
	int totalBattles = static_cast<int>(engine.battleHistory.size());
	if (totalBattles == 0)
		return;

	int maxBack = std::min(totalBattles, GRAPH_MAX_BACK);
	RenderScrollBar("##BarScroll", m_barScroll, totalBattles, maxBack);

	int startIdx = m_barScroll.offset;
	int count = std::min(GRAPH_WINDOW, m_gTotalCached - m_barScroll.offset);
	if (count <= 0)
		return;

	float xMin = m_gNums[startIdx] - 0.5f;
	float xMax = m_gNums[startIdx + count - 1] + 1.5f;

	auto C = [&](const char* key) -> ImVec4 {
		auto it = engine.settings.damageColors.find(key);
		return it != engine.settings.damageColors.end() ? it->second : ImVec4(1, 1, 1, 1);
	};

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
			{ "Melee",      "hit",       &m_gMelee },
			{ "DD",         "non-melee", &m_gDD },
			{ "DoT",        "dot",       &m_gDots },
			{ "Pet",        "pet",       &m_gPets },
			{ "Crit",       "crit",      &m_gCrits },
			{ "Heals",      "heal",      &m_gHeals },
			{ "Crit Heals", "critHeals", &m_gCritHeals },
		};

		for (int s = 0; s < numSeries; s++)
		{
			float shifted[GRAPH_WINDOW];
			for (int i = 0; i < count; i++)
				shifted[i] = m_gNums[startIdx + i] + startOff + s * barWidth;

			ImPlot::SetNextFillStyle(C(series[s].colorKey));
			ImPlot::PlotBars(series[s].label, shifted, series[s].data->data() + startIdx, count, barWidth * 0.9f);
		}

		ImPlot::EndPlot();
	}
}

void MyDPSRenderer::RenderPieChart(MyDPSEngine& engine)
{
	if (engine.sortedSessionTargets.empty())
	{
		ImGui::TextDisabled("No target data for pie chart.");
		return;
	}

	std::vector<const char*> labels;
	std::vector<double> values;
	labels.reserve(engine.sortedSessionTargets.size());
	values.reserve(engine.sortedSessionTargets.size());
	for (const auto& data : engine.sortedSessionTargets)
	{
		labels.push_back(data.name.c_str());
		values.push_back(static_cast<double>(data.totalDamage));
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
	if (engine.sortedSessionHeals.empty())
	{
		ImGui::TextDisabled("No healing data yet.");
		return;
	}

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

		for (const auto& data : engine.sortedSessionHeals)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("%s", data.name.c_str());
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(data.directHeals).c_str());
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(data.critHeals).c_str());
			ImGui::TableNextColumn(); ImGui::Text("%d", data.healCount);
			ImGui::TableNextColumn(); ImGui::Text("%s", FormatNumber(data.GetTotalHeals()).c_str());
		}

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("Healing Distribution by Player");

	std::vector<const char*> labels;
	std::vector<double> values;
	labels.reserve(engine.sortedSessionHeals.size());
	values.reserve(engine.sortedSessionHeals.size());
	for (const auto& data : engine.sortedSessionHeals)
	{
		labels.push_back(data.name.c_str());
		values.push_back(static_cast<double>(data.GetTotalHeals()));
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

	auto oldStyle = ImGuiTheme::ApplyTheme(engine.settings.themeIdx);

	auto windowName = fmt::format("DPS Config##{}", engine.charName);
	ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiCond_FirstUseEver);

	bool wasOpen = engine.showConfigWindow;
	if (ImGui::Begin(windowName.c_str(), &engine.showConfigWindow))
	{
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * engine.settings.fontScale);
		RenderConfig(engine);
		ImGui::PopFont();
	}

	ImGui::End();
	ImGuiTheme::ResetTheme(oldStyle);

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

	if (ImGui::BeginTabBar("ConfigTabs"))
	{
		if (ImGui::BeginTabItem("Display"))
		{
			if (ImGui::BeginChild("##DisplayChild", ImVec2(0, 0), ImGuiChildFlags_None))
			{
				int sizeX = static_cast<int>(ImGui::GetWindowWidth());

				BoolEntry bools[] = {
					{ "Show Type",         &s.showType },
					{ "Show Target",       &s.showTarget },
					{ "Show My Misses",    &s.showMyMisses },
					{ "Show Missed Me",    &s.showMissMe },
					{ "Show Hit Me",       &s.showHitMe },
					{ "Show Crit Heals",   &s.showCritHeals },
					{ "Show Damage Shield",&s.showDS },
					{ "Track Pet",         &s.addPet },
					{ "Show Combat Output", &engine.showCombatSpam },
					{ "Sort Newest First",  &s.sortNewest },
					{ "Auto Start",         &s.autoStart },
					{ "Lock Combat Output", &s.spamClickThrough },
				};

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

				ImGui::Spacing();
				ImGui::SetNextItemWidth(100);
				ImGui::SliderInt("Display Time (s)", &s.displayTime, 1, 60, "%d", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderInt("Battle End Delay (s)", &s.battleEndDelay, 1, 30, "%d", ImGuiSliderFlags_AlwaysClamp);

				ImGui::Spacing();
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Font Scale", &s.fontScale, 0.5f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Output Font Scale", &s.spamFontScale, 0.5f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Colors"))
		{
			if (ImGui::BeginChild("##ColorsChild", ImVec2(0, 0), ImGuiChildFlags_None))
			{
				int sizeX = static_cast<int>(ImGui::GetWindowWidth());

				ImGui::ColorEdit4("Background", &s.bgColor.x, ImGuiColorEditFlags_NoInputs);

				static const char* uiOnlyKeys[] = {
					"dmg-target", "heal-target", "dps", "duration", "avg", "total"
				};

				int col = std::max(1, sizeX / 150);
				if (ImGui::BeginTable("UI Colors", col))
				{
					ImGui::TableNextRow();
					for (const char* key : uiOnlyKeys)
					{
						auto it = s.damageColors.find(key);
						if (it != s.damageColors.end())
						{
							ImGui::TableNextColumn();
							ImGui::ColorEdit4(it->first.c_str(), &it->second.x, ImGuiColorEditFlags_NoInputs);
						}
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("FCT"))
		{
			if (ImGui::BeginChild("##FCTChild", ImVec2(0, 0), ImGuiChildFlags_None))
			{
				int sizeX = static_cast<int>(ImGui::GetWindowWidth());

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
					{ "DS",         &s.showFCT_DS },
					{ "Icons",      &s.showFCT_Icons },
					{ "Distinct Melee", &s.fctDistinctMelee },
					{ "Spell Icons", &s.fctUseSpellIcons },
				};

				int fctCol = std::max(1, sizeX / 130);
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

				ImGui::Separator();

				EnsurePickerAnimations();

				if (ImGui::BeginTable("FCT Types", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 40.0f);
					ImGui::TableSetupColumn("Icon", ImGuiTableColumnFlags_WidthFixed, 36.0f);
					ImGui::TableSetupColumn("##Reset", ImGuiTableColumnFlags_WidthFixed, 60.0f);
					ImGui::TableHeadersRow();

					for (const auto& info : GetFCTTypeInfoList())
					{
						if (info.isMeleeVerb && !s.fctDistinctMelee)
							continue;

						ImGui::PushID(info.key);
						ImGui::TableNextRow();

						ImGui::TableNextColumn();
						ImGui::TextUnformatted(info.displayName);

						ImGui::TableNextColumn();
						auto colorIt = s.damageColors.find(info.key);
						if (colorIt != s.damageColors.end())
							ImGui::ColorEdit4("##color", &colorIt->second.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

						ImGui::TableNextColumn();
						{
							auto overIt = s.fctIconOverrides.find(info.key);
							int iconID = (overIt != s.fctIconOverrides.end()) ? overIt->second.iconID : info.defaultIcon;

							ImVec2 cursorPos = ImGui::GetCursorScreenPos();
							bool drewIcon = false;
							if (iconID >= 0)
							{
								CTextureAnimation* anim = (iconID >= 500) ? m_pPickerItemAnim : m_pPickerSpellAnim;
								if (anim)
								{
									int cell = (iconID >= 500) ? (iconID - 500) : iconID;
									anim->SetCurCell(cell);
									mq::imgui::DrawTextureAnimation(anim, CXSize(24, 24));
									drewIcon = true;
								}
							}
							if (!drewIcon)
								ImGui::Dummy(ImVec2(24, 24));

							ImGui::SetCursorScreenPos(cursorPos);
							if (ImGui::InvisibleButton("##pick", ImVec2(24, 24)))
							{
								m_iconPickerOpen = true;
								m_iconPickerKey = info.key;
								m_iconPickerLabel = info.displayName;
							}
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("Click to change icon");
						}

						ImGui::TableNextColumn();
						if (ImGui::SmallButton("X"))
							s.fctIconOverrides[info.key] = { FCT_ICON_NONE, false };
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Remove icon");
						ImGui::SameLine();
						if (ImGui::SmallButton("D"))
							s.fctIconOverrides.erase(info.key);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Reset to default");

						ImGui::PopID();
					}

					ImGui::EndTable();
				}

				ImGui::Separator();

				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Base Font Size", &s.fctBaseFontSize, 12.0f, 48.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Font Scale", &s.fctFontScale, 0.5f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Icon Scale", &s.fctIconScale, 0.1f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Float Distance", &s.fctFloatDistance, 30.0f, 300.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Arc Scale", &s.fctArcScale, 0.0f, 3.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Shadow Offset", &s.fctShadowOffset, 0.0f, 5.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SetNextItemWidth(100);
				ImGui::SliderFloat("Lifetime (s)", &s.fctLifetime, 1.0f, 5.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);

				ImGui::Separator();
				ImGui::Text("Bone Anchors");

				const auto& bones = GetFCTBoneList();

				ImGui::SetNextItemWidth(150);
				if (ImGui::BeginCombo("Player Bone", GetBoneLabelByIndex(s.fctBonePlayer)))
				{
					for (int i = 0; i < static_cast<int>(bones.size()); ++i)
					{
						if (i == 6)
							ImGui::Separator();
						bool isSelected = (s.fctBonePlayer == bones[i].index);
						if (ImGui::Selectable(bones[i].label, isSelected))
							s.fctBonePlayer = bones[i].index;
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SetNextItemWidth(150);
				if (ImGui::BeginCombo("Other Bone", GetBoneLabelByIndex(s.fctBoneOther)))
				{
					for (int i = 0; i < static_cast<int>(bones.size()); ++i)
					{
						if (i == 6)
							ImGui::Separator();
						bool isSelected = (s.fctBoneOther == bones[i].index);
						if (ImGui::Selectable(bones[i].label, isSelected))
							s.fctBoneOther = bones[i].index;
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Theme"))
		{
			if (ImGui::BeginChild("##ThemeChild", ImVec2(0, 0), ImGuiChildFlags_None))
			{
				s.themeIdx = ImGuiTheme::DrawThemePicker(s.themeIdx, "Theme##DPSTheme");
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
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

void MyDPSRenderer::EnsurePickerAnimations()
{
	if (!m_pPickerSpellAnim && pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_SpellGems"))
		{
			m_pPickerSpellAnim = new CTextureAnimation();
			*m_pPickerSpellAnim = *temp;
		}
	}
	if (!m_pPickerItemAnim && pSidlMgr)
	{
		if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_DragItem"))
		{
			m_pPickerItemAnim = new CTextureAnimation();
			*m_pPickerItemAnim = *temp;
		}
	}
}

void MyDPSRenderer::RenderIconPicker(MyDPSEngine& engine)
{
	if (!m_iconPickerOpen)
		return;

	EnsurePickerAnimations();

	auto winName = fmt::format("Icon Picker - {}##FCTIconPicker", m_iconPickerLabel);
	ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin(winName.c_str(), &m_iconPickerOpen, ImGuiWindowFlags_None))
	{
		ImGui::End();
		return;
	}

	bool isItemTab = m_iconPickerShowItems;
	if (ImGui::BeginTabBar("##IconPickerTabs"))
	{
		if (ImGui::BeginTabItem("Spell Icons"))
		{
			m_iconPickerShowItems = false;
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Item Icons"))
		{
			m_iconPickerShowItems = true;
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::SetNextItemWidth(150);
	ImGui::SliderFloat("Icon Size", &m_iconPickerScale, 0.5f, 3.0f, "%.1fx", ImGuiSliderFlags_AlwaysClamp);

	int maxIcon = m_iconPickerShowItems ? 12599 : 499;
	int iconsPerPage = 500;
	int maxPage = (maxIcon / iconsPerPage) + 1;
	if (m_iconPickerPage < 1) m_iconPickerPage = 1;
	if (m_iconPickerPage > maxPage) m_iconPickerPage = maxPage;

	ImGui::SameLine();
	ImGui::SetNextItemWidth(150);
	ImGui::SliderInt("Page", &m_iconPickerPage, 1, maxPage);

	float iconSize = 40.0f * m_iconPickerScale;
	float spacing = ImGui::GetStyle().ItemSpacing.x;
	float windowWidth = ImGui::GetContentRegionAvail().x;
	int cols = std::max(1, static_cast<int>(windowWidth / (iconSize + spacing)));

	int startId = (m_iconPickerPage - 1) * iconsPerPage;
	int endId = std::min(maxIcon, startId + iconsPerPage - 1);

	CTextureAnimation* anim = m_iconPickerShowItems ? m_pPickerItemAnim : m_pPickerSpellAnim;

	bool selected = false;

	if (anim && ImGui::BeginChild("##IconGrid", ImVec2(0, 0), ImGuiChildFlags_None))
	{
		if (ImGui::BeginTable("##Icons", cols))
		{
			for (int id = startId; id <= endId && !selected; id++)
			{
				ImGui::TableNextColumn();
				ImGui::PushID(id);

				ImVec2 cursorPos = ImGui::GetCursorScreenPos();
				anim->SetCurCell(id);

				int iSize = static_cast<int>(iconSize);
				mq::imgui::DrawTextureAnimation(anim, CXSize(iSize, iSize));

				ImGui::SetCursorScreenPos(cursorPos);
				if (ImGui::InvisibleButton("##icon", ImVec2(iconSize, iconSize)))
				{
					int storedID = m_iconPickerShowItems ? (id + 500) : id;
					engine.settings.fctIconOverrides[m_iconPickerKey] = { storedID, m_iconPickerShowItems };
					m_iconPickerOpen = false;
					selected = true;
				}

				if (!selected && ImGui::IsItemHovered())
				{
					int displayID = m_iconPickerShowItems ? (id + 500) : id;
					ImGui::SetTooltip("%s Icon: %d",
						m_iconPickerShowItems ? "Item" : "Spell", displayID);
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::EndChild();
	}

	ImGui::End();
}
