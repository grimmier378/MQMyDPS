#pragma once

#include <mq/Plugin.h>

class MyDPSTargetType : public MQ2Type
{
public:
	enum class Members
	{
		Name,
		SpawnID,
		DPS,
		TotalDamage,
		HitCount,
		Duration,
		AvgDamage,
	};

	MyDPSTargetType();

	bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override;
	bool ToString(MQVarPtr VarPtr, char* Destination) override;
};

class MyDPSType : public MQ2Type
{
public:
	enum class Members
	{
		Version,
		DPS,
		TotalDamage,
		InCombat,
		BattleDPS,
		BattleDuration,
		Target,
		TargetCount,
		BattleCount,
	};

	enum class Methods
	{
		Reset,
	};

	MyDPSType();

	bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override;
	bool ToString(MQVarPtr VarPtr, char* Destination) override;

	static bool dataMyDPS(const char* szIndex, MQTypeVar& Ret);
};

extern MyDPSTargetType* pMyDPSTargetType;
extern MyDPSType* pMyDPSType;

void RegisterMyDPSTLO();
void UnregisterMyDPSTLO();
