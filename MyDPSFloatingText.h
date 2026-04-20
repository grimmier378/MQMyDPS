#pragma once

#include "MyDPSData.h"

#include <mq/Plugin.h>
#include <eqlib/graphics/CameraInterface.h>
#include <eqlib/graphics/Bones.h>
#include <eqlib/graphics/Actors.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

struct FCTEntry
{
	int         spawnID     = 0;
	std::string displayText;
	ImVec4      color       = ImVec4(1, 1, 1, 1);
	float       xOffset     = 0.0f;
	float       lifetime    = 0.0f;
	float       maxLifetime = 2.5f;
	bool        active      = false;
};

class FCTManager
{
public:
	void AddEntry(const DamageRecord& record, const MyDPSSettings& settings);
	void Update(float deltaTime);
	void Render(const MyDPSSettings& settings);
	void Clear();

private:
	static constexpr int   MAX_ENTRIES     = 64;
	static constexpr float POP_DURATION    = 0.3f;
	static constexpr float POP_START_SCALE = 1.4f;
	static constexpr float FADE_START      = 0.6f;
	static constexpr float FLOAT_DISTANCE  = 150.0f;
	static constexpr float SPREAD_RANGE    = 60.0f;

	std::vector<FCTEntry> m_entries;
	int m_nextSlot = 0;
	std::unordered_map<int, int> m_spawnHitCounter;

	static float EaseOutQuad(float t) { return t * (2.0f - t); }

	static std::string FormatNumber(int64_t value)
	{
		if (value >= 1000000)
			return fmt::format("{:.1f}m", value / 1000000.0);
		if (value >= 1000)
			return fmt::format("{:.1f}k", value / 1000.0);
		return fmt::format("{}", value);
	}

	bool IsTypeEnabled(DamageType type, const MyDPSSettings& settings) const;
	bool ProjectSpawnToScreen(int spawnID, float& outX, float& outY) const;
};

inline bool FCTManager::IsTypeEnabled(DamageType type, const MyDPSSettings& settings) const
{
	switch (type)
	{
	case DamageType::Melee:          return settings.showFCT_Melee;
	case DamageType::NonMelee:       return settings.showFCT_DD;
	case DamageType::DoT:            return settings.showFCT_DoT;
	case DamageType::PetMelee:
	case DamageType::PetNonMelee:    return settings.showFCT_Pet;
	case DamageType::Crit:           return settings.showFCT_Crit;
	case DamageType::DirectHeal:     return settings.showFCT_Heals;
	case DamageType::CritHeal:       return settings.showFCT_CritHeals;
	case DamageType::HitBy:
	case DamageType::HitByNonMelee:  return settings.showFCT_HitBy;
	default:                         return false;
	}
}

inline void FCTManager::AddEntry(const DamageRecord& record, const MyDPSSettings& settings)
{
	if (!settings.showFCT)
		return;
	if (record.targetSpawnID <= 0)
		return;
	if (record.isMiss)
		return;
	if (record.damage <= 0)
		return;
	if (!IsTypeEnabled(record.type, settings))
		return;

	if (m_entries.empty())
		m_entries.resize(MAX_ENTRIES);

	FCTEntry& entry = m_entries[m_nextSlot];
	m_nextSlot = (m_nextSlot + 1) % MAX_ENTRIES;

	entry.spawnID     = record.targetSpawnID;
	entry.lifetime    = 0.0f;
	entry.maxLifetime = settings.fctLifetime;
	entry.active      = true;

	if (record.type == DamageType::DirectHeal || record.type == DamageType::CritHeal)
		entry.displayText = fmt::format("+{}", FormatNumber(record.damage));
	else if (record.type == DamageType::HitBy || record.type == DamageType::HitByNonMelee)
		entry.displayText = fmt::format("-{}", FormatNumber(record.damage));
	else
		entry.displayText = FormatNumber(record.damage);

	const char* colorKey = DamageTypeToColorKey(record.type);
	if (record.type == DamageType::PetMelee || record.type == DamageType::PetNonMelee)
		colorKey = "pet";
	auto it = settings.damageColors.find(colorKey);
	entry.color = (it != settings.damageColors.end()) ? it->second : ImVec4(1, 1, 1, 1);

	int& counter = m_spawnHitCounter[record.targetSpawnID];
	float side = (counter % 2 == 0) ? -1.0f : 1.0f;
	float magnitude = 15.0f + (counter / 2) * 20.0f;
	entry.xOffset = side * std::min(magnitude, SPREAD_RANGE);
	counter++;
}

inline void FCTManager::Update(float deltaTime)
{
	bool anyActiveForSpawn = false;
	for (auto& entry : m_entries)
	{
		if (!entry.active)
			continue;

		entry.lifetime += deltaTime;
		if (entry.lifetime >= entry.maxLifetime)
			entry.active = false;
	}

	for (auto it = m_spawnHitCounter.begin(); it != m_spawnHitCounter.end();)
	{
		bool found = false;
		for (const auto& entry : m_entries)
		{
			if (entry.active && entry.spawnID == it->first)
			{
				found = true;
				break;
			}
		}
		if (!found)
			it = m_spawnHitCounter.erase(it);
		else
			++it;
	}
}

inline void FCTManager::Clear()
{
	for (auto& entry : m_entries)
		entry.active = false;
	m_spawnHitCounter.clear();
	m_nextSlot = 0;
}

inline bool FCTManager::ProjectSpawnToScreen(int spawnID, float& outX, float& outY) const
{
	PlayerClient* pSpawn = GetSpawnByID(spawnID);
	if (!pSpawn)
		return false;

	CCamera* camera = static_cast<eqlib::CCamera*>(pDisplay->pCamera);
	if (!camera)
		return false;

	glm::vec3 worldPos;

	CActorInterface* pActor = pSpawn->mActorClient.pActor;
	if (pActor && pActor->GetBoneByIndex(eBoneHead))
	{
		pActor->GetBoneWorldPosition(eBoneHead, reinterpret_cast<CVector3*>(&worldPos), false);
	}
	else
	{
		worldPos = glm::vec3(pSpawn->Y, pSpawn->X, pSpawn->Z + pSpawn->AvatarHeight);
	}

	glm::vec3 eyeOffset;
	pGraphicsEngine->pRender->GetEyeOffset(*reinterpret_cast<CVector3*>(&eyeOffset));
	worldPos += eyeOffset;

	float Ez = glm::dot(worldPos, glm::vec3(
		camera->worldToEyeCoef[0][2],
		camera->worldToEyeCoef[1][2],
		camera->worldToEyeCoef[2][2]));

	if (Ez <= 0.0f)
		return false;

	float Ex = glm::dot(worldPos, camera->worldToEyeXAxisCot);
	float Ey = glm::dot(worldPos, camera->worldToEyeYAxisCotAspect);

	float reci = 1.0f / Ez;
	outX = Ex * reci * camera->halfRenderWidth + camera->halfRenderWidth + camera->left;
	outY = -Ey * reci * camera->halfRenderHeight + camera->halfRenderHeight + camera->top;

	return true;
}

inline void FCTManager::Render(const MyDPSSettings& settings)
{
	if (!settings.showFCT)
		return;

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	ImFont* font = ImGui::GetFont();
	if (!font || !drawList)
		return;

	for (const auto& entry : m_entries)
	{
		if (!entry.active)
			continue;

		float screenX = 0, screenY = 0;
		if (!ProjectSpawnToScreen(entry.spawnID, screenX, screenY))
			continue;

		float t = entry.lifetime / entry.maxLifetime;

		float popT = std::min(entry.lifetime / POP_DURATION, 1.0f);
		float scale = POP_START_SCALE + (1.0f - POP_START_SCALE) * EaseOutQuad(popT);

		float floatY = -FLOAT_DISTANCE * EaseOutQuad(t);

		float alpha = 1.0f;
		if (t > FADE_START)
		{
			float fadeT = (t - FADE_START) / (1.0f - FADE_START);
			alpha = 1.0f - fadeT;
		}

		float fontSize = settings.fctBaseFontSize * settings.fctFontScale * scale;
		ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0, entry.displayText.c_str());

		float x = screenX + entry.xOffset - textSize.x * 0.5f;
		float y = screenY + floatY;

		float shadowOff = settings.fctShadowOffset * (fontSize / settings.fctBaseFontSize);
		ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, alpha * 0.8f));
		drawList->AddText(font, fontSize, ImVec2(x + shadowOff, y + shadowOff), shadowCol, entry.displayText.c_str());

		ImU32 col = ImGui::ColorConvertFloat4ToU32(
			ImVec4(entry.color.x, entry.color.y, entry.color.z, entry.color.w * alpha));
		drawList->AddText(font, fontSize, ImVec2(x, y), col, entry.displayText.c_str());
	}
}
