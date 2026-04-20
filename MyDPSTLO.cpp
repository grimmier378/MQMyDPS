#include "MyDPSTLO.h"
#include "MQMyDPS.h"

MyDPSTargetType* pMyDPSTargetType = nullptr;
MyDPSType* pMyDPSType = nullptr;

static const TargetDamageData* FindTargetBySpawnID(int spawnID)
{
	if (!g_dpsEngine || spawnID == 0)
		return nullptr;

	auto it = g_dpsEngine->currentTargets.find(spawnID);
	if (it != g_dpsEngine->currentTargets.end())
		return &it->second;
	return nullptr;
}

static const TargetDamageData* FindTargetByName(const char* name)
{
	if (!g_dpsEngine || !name || !name[0])
		return nullptr;

	for (const auto& [id, data] : g_dpsEngine->currentTargets)
	{
		if (data.name == name)
			return &data;
	}
	return nullptr;
}

MyDPSTargetType::MyDPSTargetType() : MQ2Type("MyDPSTarget")
{
	ScopedTypeMember(Members, Name);
	ScopedTypeMember(Members, SpawnID);
	ScopedTypeMember(Members, DPS);
	ScopedTypeMember(Members, TotalDamage);
	ScopedTypeMember(Members, HitCount);
	ScopedTypeMember(Members, Duration);
	ScopedTypeMember(Members, AvgDamage);
}

bool MyDPSTargetType::GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest)
{
	MQTypeMember* pMember = FindMember(Member);
	if (!pMember || !g_dpsEngine)
		return false;

	const TargetDamageData* data = FindTargetBySpawnID(VarPtr.Int);
	if (!data)
		return false;

	switch (static_cast<Members>(pMember->ID))
	{
	case Members::Name:
		strcpy_s(DataTypeTemp, MAX_STRING, data->name.c_str());
		Dest.Ptr = &DataTypeTemp[0];
		Dest.Type = mq::datatypes::pStringType;
		return true;

	case Members::SpawnID:
		Dest.Int = data->spawnID;
		Dest.Type = mq::datatypes::pIntType;
		return true;

	case Members::DPS:
		Dest.Float = data->GetDPS();
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::TotalDamage:
		Dest.Int64 = data->totalDamage;
		Dest.Type = mq::datatypes::pInt64Type;
		return true;

	case Members::HitCount:
		Dest.Int = data->hitCount;
		Dest.Type = mq::datatypes::pIntType;
		return true;

	case Members::Duration:
		Dest.Float = data->GetDurationSeconds();
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::AvgDamage:
		Dest.Float = data->GetAvgDamage();
		Dest.Type = mq::datatypes::pFloatType;
		return true;
	}

	return false;
}

bool MyDPSTargetType::ToString(MQVarPtr VarPtr, char* Destination)
{
	const TargetDamageData* data = FindTargetBySpawnID(VarPtr.Int);
	if (!data)
		return false;

	strcpy_s(Destination, MAX_STRING, data->name.c_str());
	return true;
}

MyDPSType::MyDPSType() : MQ2Type("MyDPS")
{
	ScopedTypeMember(Members, Version);
	ScopedTypeMember(Members, DPS);
	ScopedTypeMember(Members, TotalDamage);
	ScopedTypeMember(Members, InCombat);
	ScopedTypeMember(Members, BattleDPS);
	ScopedTypeMember(Members, BattleDuration);
	ScopedTypeMember(Members, Target);
	ScopedTypeMember(Members, TargetCount);
	ScopedTypeMember(Members, BattleCount);
	ScopedTypeMethod(Methods, Reset);
}

bool MyDPSType::GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest)
{
	MQTypeMember* pMethod = FindMethod(Member);
	if (pMethod)
	{
		switch (static_cast<Methods>(pMethod->ID))
		{
		case Methods::Reset:
			if (g_dpsEngine)
				g_dpsEngine->ResetAll();
			Dest.Set(true);
			Dest.Type = mq::datatypes::pBoolType;
			return true;
		}
	}

	MQTypeMember* pMember = FindMember(Member);
	if (!pMember || !g_dpsEngine)
		return false;

	switch (static_cast<Members>(pMember->ID))
	{
	case Members::Version:
		Dest.Float = 0.1f;
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::DPS:
		Dest.Float = g_dpsEngine->GetSessionDPS();
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::TotalDamage:
		Dest.Int64 = g_dpsEngine->sessionDamage;
		Dest.Type = mq::datatypes::pInt64Type;
		return true;

	case Members::InCombat:
		Dest.Set(g_dpsEngine->inCombat);
		Dest.Type = mq::datatypes::pBoolType;
		return true;

	case Members::BattleDPS:
		Dest.Float = g_dpsEngine->GetBattleDPS();
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::BattleDuration:
		Dest.Float = g_dpsEngine->GetBattleDuration();
		Dest.Type = mq::datatypes::pFloatType;
		return true;

	case Members::Target:
		if (Index && Index[0])
		{
			const TargetDamageData* data = nullptr;

			char* end = nullptr;
			long idx = strtol(Index, &end, 10);
			if (end != Index && *end == '\0')
				data = FindTargetBySpawnID(static_cast<int>(idx));
			else
				data = FindTargetByName(Index);

			if (data)
			{
				Dest.Int = data->spawnID;
				Dest.Type = pMyDPSTargetType;
				return true;
			}
		}
		return false;

	case Members::TargetCount:
		Dest.Int = static_cast<int>(g_dpsEngine->currentTargets.size());
		Dest.Type = mq::datatypes::pIntType;
		return true;

	case Members::BattleCount:
		Dest.Int = static_cast<int>(g_dpsEngine->battleHistory.size());
		Dest.Type = mq::datatypes::pIntType;
		return true;
	}

	return false;
}

bool MyDPSType::ToString(MQVarPtr VarPtr, char* Destination)
{
	strcpy_s(Destination, MAX_STRING, "MQMyDPS");
	return true;
}

bool MyDPSType::dataMyDPS(const char* szIndex, MQTypeVar& Ret)
{
	Ret.DWord = 1;
	Ret.Type = pMyDPSType;
	return true;
}

void RegisterMyDPSTLO()
{
	pMyDPSTargetType = new MyDPSTargetType();
	pMyDPSType = new MyDPSType();
	AddTopLevelObject("MyDPS", MyDPSType::dataMyDPS);
}

void UnregisterMyDPSTLO()
{
	RemoveTopLevelObject("MyDPS");

	delete pMyDPSType;
	pMyDPSType = nullptr;

	delete pMyDPSTargetType;
	pMyDPSTargetType = nullptr;
}
