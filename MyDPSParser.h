#pragma once

#include "MyDPSData.h"

#include <optional>
#include <string_view>

class MyDPSParser
{
public:
	void SetCharName(const std::string& name) { m_charName = name; }
	void SetPetName(const std::string& name)  { m_petName = name; }

	std::optional<DamageRecord> Parse(const char* line, DWORD color);

private:
	bool TryParseMelee(std::string_view line, DamageRecord& out);
	bool TryParseMiss(std::string_view line, DamageRecord& out);
	bool TryParseNonMelee(std::string_view line, DamageRecord& out);
	bool TryParseCrit(std::string_view line, DamageRecord& out);
	bool TryParseDeadlyStrike(std::string_view line, DamageRecord& out);
	bool TryParseDoT(std::string_view line, DamageRecord& out);
	bool TryParseDamageShield(std::string_view line, DamageRecord& out);
	bool TryParseHitBy(std::string_view line, DamageRecord& out);
	bool TryParseMissedMe(std::string_view line, DamageRecord& out);
	bool TryParseCritHeal(std::string_view line, DamageRecord& out);
	bool TryParseHitByNonMelee(std::string_view line, DamageRecord& out);
	bool TryParseDirectHeal(std::string_view line, DamageRecord& out);
	bool TryParseHealedBy(std::string_view line, DamageRecord& out);
	bool TryParsePetMelee(std::string_view line, DamageRecord& out);
	bool TryParsePetNonMelee(std::string_view line, DamageRecord& out);

	int64_t ExtractNumber(std::string_view text, size_t start, size_t end);

	std::string m_charName;
	std::string m_petName;
};
