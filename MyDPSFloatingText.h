#pragma once

#include "MyDPSData.h"

#include <mq/Plugin.h>
#include <mq/imgui/Widgets.h>
#include <eqlib/graphics/CameraInterface.h>
#include <eqlib/graphics/Bones.h>
#include <eqlib/graphics/Actors.h>

#include <chrono>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct FCTBoneInfo
{
	int         index;
	const char* label;
};

inline const std::vector<FCTBoneInfo>& GetFCTBoneList()
{
	static const std::vector<FCTBoneInfo> list = {
		{ 0,  "Head"           },
		{ 11, "Chest"          },
		{ 14, "Pelvis"         },
		{ 20, "Legs"           },
		{ 21, "Left Boot"      },
		{ 22, "Right Boot"     },
		{ 1,  "Helm"           },
		{ 2,  "Guild"          },
		{ 3,  "Primary"        },
		{ 4,  "Secondary"      },
		{ 5,  "Shield"         },
		{ 6,  "Blood Spurt"    },
		{ 7,  "Tunic"          },
		{ 8,  "Hair"           },
		{ 9,  "Beard"          },
		{ 10, "Eyebrows"       },
		{ 12, "Left Bracer"    },
		{ 13, "Right Bracer"   },
		{ 15, "Spell"          },
		{ 16, "Camera"         },
		{ 17, "Arms"           },
		{ 18, "Left Glove"     },
		{ 19, "Right Glove"    },
		{ 23, "Torch"          },
		{ 24, "Facial"         },
		{ 25, "Tattoo"         },
		{ 26, "Left Shoulder"  },
		{ 27, "Right Shoulder" },
	};
	return list;
}

inline const char* GetBoneLabelByIndex(int boneIndex)
{
	for (const auto& b : GetFCTBoneList())
	{
		if (b.index == boneIndex)
			return b.label;
	}
	return "Unknown";
}

struct FCTEntry
{
	int         spawnID     = 0;
	std::string displayText;
	ImVec4      color       = ImVec4(1, 1, 1, 1);
	float       dirAngle    = 0.0f;
	float       arcAmount   = 0.0f;
	float       lifetime    = 0.0f;
	float       maxLifetime = 2.5f;
	bool        active      = false;
	int         iconCellID  = -1;
};

class FCTManager
{
public:
	~FCTManager();

	void AddEntry(const DamageRecord& record, const MyDPSSettings& settings);
	void Update(float deltaTime);
	void Render(const MyDPSSettings& settings);
	void Clear();

private:
	static constexpr int   MAX_ENTRIES     = 64;
	static constexpr float POP_DURATION    = 0.3f;
	static constexpr float POP_START_SCALE = 1.4f;
	static constexpr float FADE_START       = 0.6f;
	static constexpr int   NUM_ANGLE_SLOTS  = 8;
	static constexpr float ARC_BASE         = 20.0f;

	std::vector<FCTEntry> m_entries;
	int m_nextSlot = 0;
	std::unordered_map<int, int> m_spawnAngleSlot;
	std::mt19937 m_rng{ std::random_device{}() };
	CTextureAnimation* m_pSpellIconAnim = nullptr;
	CTextureAnimation* m_pItemIconAnim  = nullptr;

	void EnsureIconAnimation()
	{
		if (!m_pSpellIconAnim && pSidlMgr)
		{
			if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_SpellGems"))
			{
				m_pSpellIconAnim = new CTextureAnimation();
				*m_pSpellIconAnim = *temp;
			}
		}
		if (!m_pItemIconAnim && pSidlMgr)
		{
			if (CTextureAnimation* temp = pSidlMgr->FindAnimation("A_DragItem"))
			{
				m_pItemIconAnim = new CTextureAnimation();
				*m_pItemIconAnim = *temp;
			}
		}
	}

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
	bool ProjectSpawnToScreen(int spawnID, float& outX, float& outY, int bonePlayer, int boneOther) const;
};

inline FCTManager::~FCTManager()
{
	delete m_pSpellIconAnim;
	m_pSpellIconAnim = nullptr;
	delete m_pItemIconAnim;
	m_pItemIconAnim = nullptr;
}

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
	case DamageType::DamageShield:   return settings.showFCT_DS;
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
	entry.iconCellID  = record.spellIconID;

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
	if (record.type == DamageType::Melee && settings.fctDistinctMelee && !record.attackVerb.empty())
	{
		auto verbIt = settings.damageColors.find(record.attackVerb);
		if (verbIt != settings.damageColors.end())
			it = verbIt;
	}
	entry.color = (it != settings.damageColors.end()) ? it->second : ImVec4(1, 1, 1, 1);

	static constexpr float angleSlots[NUM_ANGLE_SLOTS] = {
		-1.5708f,            // -90°   straight up
		-1.1781f,            // -67.5° up-right
		-1.9635f,            // -112.5° up-left
		-0.7854f,            // -45°   diagonal right
		-2.3562f,            // -135°  diagonal left
		-0.5236f,            // -30°   steep right
		-2.6180f,            // -150°  steep left
		-1.3744f,            // -78.75° slight right of up
	};

	int& slot = m_spawnAngleSlot[record.targetSpawnID];
	entry.dirAngle = angleSlots[slot % NUM_ANGLE_SLOTS];
	slot++;

	std::uniform_real_distribution<float> arcDist(0.5f, 1.0f);
	float arcSign = (slot % 2 == 0) ? 1.0f : -1.0f;
	entry.arcAmount = arcSign * ARC_BASE * arcDist(m_rng) * settings.fctArcScale;
}

inline void FCTManager::Update(float deltaTime)
{
	for (auto& entry : m_entries)
	{
		if (!entry.active)
			continue;

		entry.lifetime += deltaTime;
		if (entry.lifetime >= entry.maxLifetime)
			entry.active = false;
	}

	for (auto it = m_spawnAngleSlot.begin(); it != m_spawnAngleSlot.end();)
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
			it = m_spawnAngleSlot.erase(it);
		else
			++it;
	}
}

inline void FCTManager::Clear()
{
	for (auto& entry : m_entries)
		entry.active = false;
	m_spawnAngleSlot.clear();
	m_nextSlot = 0;
}

inline bool FCTManager::ProjectSpawnToScreen(int spawnID, float& outX, float& outY, int bonePlayer, int boneOther) const
{
	PlayerClient* pSpawn = GetSpawnByID(spawnID);
	if (!pSpawn)
		return false;

	CCamera* camera = static_cast<eqlib::CCamera*>(pDisplay->pCamera);
	if (!camera)
		return false;

	glm::vec3 worldPos;

	CActorInterface* pActor = pSpawn->mActorClient.pActor;
	EBones boneIdx = static_cast<EBones>(
		(pSpawn->Type == SPAWN_PLAYER) ? bonePlayer : boneOther);
	if (pActor && pActor->GetBoneByIndex(boneIdx))
	{
		pActor->GetBoneWorldPosition(boneIdx, reinterpret_cast<CVector3*>(&worldPos), false);
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

	if (settings.showFCT_Icons)
		EnsureIconAnimation();

	for (const auto& entry : m_entries)
	{
		if (!entry.active)
			continue;

		float screenX = 0, screenY = 0;
		if (!ProjectSpawnToScreen(entry.spawnID, screenX, screenY,
				settings.fctBonePlayer, settings.fctBoneOther))
			continue;

		float t = entry.lifetime / entry.maxLifetime;

		float popT = std::min(entry.lifetime / POP_DURATION, 1.0f);
		float scale = POP_START_SCALE + (1.0f - POP_START_SCALE) * EaseOutQuad(popT);

		float distance = settings.fctFloatDistance * EaseOutQuad(t);
		float dx = cosf(entry.dirAngle) * distance;
		float dy = sinf(entry.dirAngle) * distance;

		float arcT = sinf(3.14159f * t);
		float perpX = -sinf(entry.dirAngle) * entry.arcAmount * arcT;
		float perpY =  cosf(entry.dirAngle) * entry.arcAmount * arcT;

		float floatX = dx + perpX;
		float floatY = dy + perpY;

		float alpha = 1.0f;
		if (t > FADE_START)
		{
			float fadeT = (t - FADE_START) / (1.0f - FADE_START);
			alpha = 1.0f - fadeT;
		}

		float fontSize = settings.fctBaseFontSize * settings.fctFontScale * scale;
		ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0, entry.displayText.c_str());

		bool isItemIcon = entry.iconCellID >= 500;
		CTextureAnimation* iconAnim = isItemIcon ? m_pItemIconAnim : m_pSpellIconAnim;
		bool drawIcon = settings.showFCT_Icons && entry.iconCellID >= 0 && iconAnim;
		float iconSize = fontSize * settings.fctIconScale;
		float iconPad  = drawIcon ? 2.0f * scale : 0.0f;
		float totalWidth = textSize.x + (drawIcon ? (iconSize + iconPad) : 0.0f);

		float blockX = screenX + floatX - totalWidth * 0.5f;
		float y = screenY + floatY;

		float iconX = blockX;
		float textX = drawIcon ? (blockX + iconSize + iconPad) : blockX;

		float shadowOff = settings.fctShadowOffset * (fontSize / settings.fctBaseFontSize);
		ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, alpha * 0.8f));
		drawList->AddText(font, fontSize, ImVec2(textX + shadowOff, y + shadowOff), shadowCol, entry.displayText.c_str());

		ImU32 col = ImGui::ColorConvertFloat4ToU32(
			ImVec4(entry.color.x, entry.color.y, entry.color.z, entry.color.w * alpha));
		drawList->AddText(font, fontSize, ImVec2(textX, y), col, entry.displayText.c_str());

		if (drawIcon)
		{
			int cellID = isItemIcon ? (entry.iconCellID - 500) : entry.iconCellID;
			iconAnim->SetCurCell(cellID);
			uint8_t alpha255 = static_cast<uint8_t>(alpha * 255.0f);
			MQColor tint(255, 255, 255, alpha255);
			int iSize = static_cast<int>(iconSize);
			mq::imgui::DrawTextureAnimation(
				drawList,
				iconAnim,
				CXPoint(static_cast<int>(iconX), static_cast<int>(y)),
				CXSize(iSize, iSize),
				tint);
		}
	}
}
