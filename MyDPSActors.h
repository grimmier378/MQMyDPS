#pragma once

#include "MyDPSData.h"

#include <mq/api/ActorAPI.h>

#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MyDPSEngine;

enum class PeerFilterMode : int
{
	All        = 0,
	SameServer = 1,
	SameGroup  = 2,
	SameRaid   = 3,
};

struct PeerBattle
{
	int     battleNumber    = 0;
	int64_t totalDamage     = 0;
	int64_t critDamage      = 0;
	int64_t dotDamage       = 0;
	int64_t petDamage       = 0;
	int64_t nonMeleeDamage  = 0;
	int64_t dsDamage        = 0;
	int64_t directHeals     = 0;
	int64_t critHeals       = 0;
	int     hitCount        = 0;
	float   durationSeconds = 0.0f;
	float   dps             = 0.0f;
	float   avgDamage       = 0.0f;
	int64_t startTimeMs     = 0;
};

struct PeerRecord
{
	std::string server;
	std::string character;
	std::string classShort;
	bool        isSelf = false;

	bool    inCombat      = false;
	float   liveDPS       = 0.0f;
	float   liveDuration  = 0.0f;
	int64_t liveTotal     = 0;
	int64_t liveCrit      = 0;
	int64_t liveDot       = 0;
	int64_t livePet       = 0;
	int64_t liveNonMelee  = 0;
	int64_t liveDS        = 0;
	int64_t liveHeals     = 0;
	int64_t liveCritHeals = 0;
	int     liveHits      = 0;

	int64_t sessionDamage = 0;
	int64_t sessionHeals  = 0;
	float   sessionDPS    = 0.0f;
	int     battleCount   = 0;

	std::deque<PeerBattle> battles;

	ImVec4  color = ImVec4(1, 1, 1, 1);
	std::chrono::steady_clock::time_point lastSeen;
};

class MyDPSActors
{
public:
	void Init(MyDPSEngine* engine, const std::string& server, const std::string& character);
	void Shutdown();
	void Pulse();

	void PublishFinalizedBattle(const BattleData& battle);

	bool IsValid() const { return m_valid; }
	void DebugDump() const;

	const std::unordered_map<std::string, PeerRecord>& Peers() const { return m_peers; }

	std::vector<const PeerRecord*> VisiblePeers(PeerFilterMode mode);

private:
	void Log(const std::string& msg) const;
	void LogDebug(const std::string& msg) const;
	void OnReceive(const std::shared_ptr<mq::postoffice::Message>& msg);
	void PublishLive();
	void PublishSession();
	void PruneStale();
	void BuildSelfRecord();

	mq::postoffice::DropboxAPI m_dropbox{};
	bool         m_valid  = false;
	MyDPSEngine* m_engine = nullptr;
	std::string  m_server;
	std::string  m_character;

	std::unordered_map<std::string, PeerRecord> m_peers;
	PeerRecord                                  m_selfRecord;

	std::chrono::steady_clock::time_point m_nextLive;
	std::chrono::steady_clock::time_point m_nextSession;
	std::chrono::steady_clock::time_point m_nextPrune;

	int m_sentCount     = 0;
	int m_recvCount     = 0;
	int m_parseFailCount = 0;
	int m_selfIgnoredCount = 0;
};

std::string PeerKey(const std::string& server, const std::string& character);
ImVec4 PeerColorForName(const std::string& name);
