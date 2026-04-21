#include "MyDPSParser.h"

#include <charconv>

std::optional<DamageRecord> MyDPSParser::Parse(const char* line, DWORD color)
{
	if (!line || !line[0])
		return std::nullopt;

	std::string_view sv(line);
	DamageRecord rec;
	rec.timestamp = std::chrono::steady_clock::now();

	if (TryParseMelee(sv, rec))       return rec;
	if (TryParseNonMelee(sv, rec))    return rec;
	if (TryParseCrit(sv, rec))        return rec;
	if (TryParseDeadlyStrike(sv, rec))return rec;
	if (TryParseDoT(sv, rec))         return rec;
	if (TryParseMiss(sv, rec))        return rec;
	if (TryParseDamageShield(sv, rec))return rec;
	if (TryParseHitBy(sv, rec))       return rec;
	if (TryParseMissedMe(sv, rec))    return rec;
	if (TryParseCritHeal(sv, rec))    return rec;
	if (TryParseDirectHeal(sv, rec))  return rec;
	if (TryParseHealedBy(sv, rec))    return rec;
	if (TryParseHitByNonMelee(sv, rec)) return rec;

	if (!m_petName.empty())
	{
		if (TryParsePetMelee(sv, rec))    return rec;
		if (TryParsePetNonMelee(sv, rec)) return rec;
	}

	return std::nullopt;
}

bool MyDPSParser::TryParseMelee(std::string_view line, DamageRecord& out)
{
	if (line.substr(0, 4) != "You ")
		return false;

	auto forPos = line.find(" for ");
	auto ptsPos = line.find(" points of damage");
	if (forPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;
	if (ptsPos < forPos)
		return false;

	auto verbAndTarget = line.substr(4, forPos - 4);
	auto spacePos = verbAndTarget.find(' ');
	if (spacePos == std::string_view::npos)
		return false;

	out.attackVerb = std::string(verbAndTarget.substr(0, spacePos));
	out.targetName = std::string(verbAndTarget.substr(spacePos + 1));
	out.damage = ExtractNumber(line, forPos + 5, ptsPos);
	out.type = DamageType::Melee;
	return true;
}

bool MyDPSParser::TryParseMiss(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view prefix = "You try to ";
	if (line.substr(0, prefix.size()) != prefix)
		return false;

	constexpr std::string_view suffix = ", but miss!";
	auto suffixPos = line.find(suffix);
	if (suffixPos == std::string_view::npos)
		return false;

	auto verbAndTarget = line.substr(prefix.size(), suffixPos - prefix.size());
	auto spacePos = verbAndTarget.find(' ');
	if (spacePos == std::string_view::npos)
		return false;

	out.attackVerb = std::string(verbAndTarget.substr(0, spacePos));
	out.targetName = std::string(verbAndTarget.substr(spacePos + 1));
	out.damage = 0;
	out.isMiss = true;
	out.type = DamageType::Miss;
	return true;
}

bool MyDPSParser::TryParseNonMelee(std::string_view line, DamageRecord& out)
{
	if (m_charName.empty())
		return false;

	std::string prefix = m_charName + " hit ";
	if (line.substr(0, prefix.size()) != prefix)
		return false;

	constexpr std::string_view marker = " for ";
	constexpr std::string_view suffix = " points of non-melee damage";

	auto forPos = line.find(marker, prefix.size());
	auto ptsPos = line.find(suffix);
	if (forPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;

	out.targetName = std::string(line.substr(prefix.size(), forPos - prefix.size()));
	out.damage = ExtractNumber(line, forPos + marker.size(), ptsPos);
	out.attackVerb = "non-melee";
	out.type = DamageType::NonMelee;
	return true;
}

bool MyDPSParser::TryParseCrit(std::string_view line, DamageRecord& out)
{
	bool found = false;
	if (line.find("You score a critical hit!") != std::string_view::npos)
		found = true;
	else if (line.find("You deliver a critical blast!") != std::string_view::npos)
		found = true;

	if (!found)
		return false;

	auto openParen = line.rfind('(');
	auto closeParen = line.rfind(')');
	if (openParen == std::string_view::npos || closeParen == std::string_view::npos || closeParen <= openParen)
		return false;

	out.damage = ExtractNumber(line, openParen + 1, closeParen);
	out.attackVerb = "crit";
	out.type = DamageType::Crit;
	return true;
}

bool MyDPSParser::TryParseDeadlyStrike(std::string_view line, DamageRecord& out)
{
	if (m_charName.empty())
		return false;

	std::string pattern = m_charName + " scores a Deadly Strike!";
	if (line.find(pattern) == std::string_view::npos)
		return false;

	auto openParen = line.rfind('(');
	auto closeParen = line.rfind(')');
	if (openParen == std::string_view::npos || closeParen == std::string_view::npos || closeParen <= openParen)
		return false;

	out.damage = ExtractNumber(line, openParen + 1, closeParen);
	out.attackVerb = "deadly";
	out.type = DamageType::Crit;
	return true;
}

bool MyDPSParser::TryParseDoT(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view hasTaken = " has taken ";
	constexpr std::string_view fromYour = " damage from your ";

	auto takenPos = line.find(hasTaken);
	auto fromPos = line.find(fromYour);
	if (takenPos == std::string_view::npos || fromPos == std::string_view::npos)
		return false;
	if (fromPos < takenPos)
		return false;

	out.targetName = std::string(line.substr(0, takenPos));
	out.damage = ExtractNumber(line, takenPos + hasTaken.size(), fromPos);
	out.spellName = std::string(line.substr(fromPos + fromYour.size()));

	if (!out.spellName.empty() && out.spellName.back() == '.')
		out.spellName.pop_back();

	out.attackVerb = "dot";
	out.type = DamageType::DoT;
	return true;
}

bool MyDPSParser::TryParseDamageShield(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view marker = " was hit by non-melee for ";
	constexpr std::string_view suffix = " points of damage";

	auto markerPos = line.find(marker);
	auto ptsPos = line.find(suffix);
	if (markerPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;

	out.targetName = std::string(line.substr(0, markerPos));
	out.damage = ExtractNumber(line, markerPos + marker.size(), ptsPos);
	out.attackVerb = "dShield";
	out.type = DamageType::DamageShield;
	return true;
}

bool MyDPSParser::TryParseHitBy(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view youFor = " YOU for ";
	constexpr std::string_view suffix = " points of damage";

	auto youPos = line.find(youFor);
	auto ptsPos = line.find(suffix);
	if (youPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;
	if (ptsPos < youPos)
		return false;

	if (line.find(" non-melee ") != std::string_view::npos)
		return false;

	auto beforeYou = line.substr(0, youPos);
	auto spacePos = beforeYou.rfind(' ');
	if (spacePos == std::string_view::npos)
		return false;

	out.targetName = std::string(beforeYou.substr(0, spacePos));
	out.attackVerb = std::string(beforeYou.substr(spacePos + 1));
	out.damage = ExtractNumber(line, youPos + youFor.size(), ptsPos);
	out.type = DamageType::HitBy;
	return true;
}

bool MyDPSParser::TryParseMissedMe(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view tryTo = " tries to ";
	constexpr std::string_view suffix = " YOU, but misses!";

	auto tryPos = line.find(tryTo);
	auto missPos = line.find(suffix);
	if (tryPos == std::string_view::npos || missPos == std::string_view::npos)
		return false;

	out.targetName = std::string(line.substr(0, tryPos));
	out.attackVerb = std::string(line.substr(tryPos + tryTo.size(), missPos - (tryPos + tryTo.size())));
	out.damage = 0;
	out.isMiss = true;
	out.type = DamageType::MissedMe;
	return true;
}

bool MyDPSParser::TryParseCritHeal(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view marker = "You perform an exceptional heal!";
	if (line.find(marker) == std::string_view::npos)
		return false;

	auto openParen = line.rfind('(');
	auto closeParen = line.rfind(')');
	if (openParen == std::string_view::npos || closeParen == std::string_view::npos || closeParen <= openParen)
		return false;

	out.damage = ExtractNumber(line, openParen + 1, closeParen);
	out.attackVerb = "critHeal";
	out.type = DamageType::CritHeal;
	return true;
}

bool MyDPSParser::TryParseHitByNonMelee(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view suffix = " points of non-melee damage";
	auto ptsPos = line.find(suffix);
	if (ptsPos == std::string_view::npos)
		return false;

	constexpr std::string_view youFor = " YOU for ";
	auto youPos = line.find(youFor);
	if (youPos == std::string_view::npos)
		return false;

	auto beforeYou = line.substr(0, youPos);
	auto hitPos = beforeYou.find(" hit ");
	if (hitPos == std::string_view::npos)
		return false;

	out.targetName = std::string(beforeYou.substr(0, hitPos));
	out.damage = ExtractNumber(line, youPos + youFor.size(), ptsPos);
	out.attackVerb = "non-melee";
	out.type = DamageType::HitByNonMelee;
	return true;
}

bool MyDPSParser::TryParsePetMelee(std::string_view line, DamageRecord& out)
{
	if (m_petName.empty())
		return false;

	std::string prefix = m_petName + " ";
	if (line.substr(0, prefix.size()) != prefix)
		return false;

	auto forPos = line.find(" for ");
	auto ptsPos = line.find(" points of damage");
	if (forPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;
	if (ptsPos < forPos)
		return false;

	auto verbAndTarget = line.substr(prefix.size(), forPos - prefix.size());
	auto spacePos = verbAndTarget.find(' ');
	if (spacePos == std::string_view::npos)
		return false;

	out.attackVerb = m_petName + " " + std::string(verbAndTarget.substr(0, spacePos));
	out.targetName = std::string(verbAndTarget.substr(spacePos + 1));
	out.damage = ExtractNumber(line, forPos + 5, ptsPos);
	out.type = DamageType::PetMelee;
	return true;
}

bool MyDPSParser::TryParsePetNonMelee(std::string_view line, DamageRecord& out)
{
	if (m_petName.empty())
		return false;

	std::string prefix = m_petName + " hit ";
	if (line.substr(0, prefix.size()) != prefix)
		return false;

	constexpr std::string_view marker = " for ";
	constexpr std::string_view suffix = " points of non-melee damage";

	auto forPos = line.find(marker, prefix.size());
	auto ptsPos = line.find(suffix);
	if (forPos == std::string_view::npos || ptsPos == std::string_view::npos)
		return false;

	out.targetName = std::string(line.substr(prefix.size(), forPos - prefix.size()));
	out.damage = ExtractNumber(line, forPos + marker.size(), ptsPos);
	out.attackVerb = m_petName + " non-melee";
	out.type = DamageType::PetNonMelee;
	return true;
}

bool MyDPSParser::TryParseDirectHeal(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view prefix = "You have healed ";
	if (line.substr(0, prefix.size()) != prefix)
		return false;

	constexpr std::string_view marker = " for ";
	auto forPos = line.find(marker, prefix.size());
	if (forPos == std::string_view::npos)
		return false;

	out.targetName = std::string(line.substr(prefix.size(), forPos - prefix.size()));

	constexpr std::string_view hitPts = " hit points";
	constexpr std::string_view withYour = " with your ";
	auto hitPtsPos = line.find(hitPts, forPos);
	auto ptsPos = line.find(" points.", forPos);

	if (hitPtsPos != std::string_view::npos)
	{
		out.damage = ExtractNumber(line, forPos + marker.size(), hitPtsPos);

		auto withPos = line.find(withYour, hitPtsPos);
		if (withPos != std::string_view::npos)
		{
			auto spellStart = withPos + withYour.size();
			auto spellEnd = line.size();
			if (spellEnd > 0 && line[spellEnd - 1] == '.')
				spellEnd--;
			out.spellName = std::string(line.substr(spellStart, spellEnd - spellStart));
		}
	}
	else if (ptsPos != std::string_view::npos)
	{
		out.damage = ExtractNumber(line, forPos + marker.size(), ptsPos);
	}
	else
	{
		return false;
	}

	out.attackVerb = "heal";
	out.type = DamageType::DirectHeal;
	return true;
}

bool MyDPSParser::TryParseHealedBy(std::string_view line, DamageRecord& out)
{
	constexpr std::string_view healedYou = " healed you for ";
	auto healPos = line.find(healedYou);
	if (healPos == std::string_view::npos)
		return false;

	out.targetName = m_charName;

	constexpr std::string_view hitPts = " hit points";
	auto hitPtsPos = line.find(hitPts, healPos + healedYou.size());
	if (hitPtsPos == std::string_view::npos)
		return false;

	out.damage = ExtractNumber(line, healPos + healedYou.size(), hitPtsPos);
	if (out.damage <= 0)
		return false;

	constexpr std::string_view byMarker = " by ";
	auto byPos = line.find(byMarker, hitPtsPos);
	if (byPos != std::string_view::npos)
	{
		auto spellStart = byPos + byMarker.size();
		auto spellEnd = line.size();
		if (spellEnd > 0 && line[spellEnd - 1] == '.')
			spellEnd--;
		out.spellName = std::string(line.substr(spellStart, spellEnd - spellStart));
	}

	out.attackVerb = "heal";
	out.type = DamageType::DirectHeal;
	return true;
}

int64_t MyDPSParser::ExtractNumber(std::string_view text, size_t start, size_t end)
{
	if (start >= end || end > text.size())
		return 0;

	auto numStr = text.substr(start, end - start);
	int64_t result = 0;
	auto [ptr, ec] = std::from_chars(numStr.data(), numStr.data() + numStr.size(), result);
	if (ec != std::errc())
		return 0;
	return result;
}
