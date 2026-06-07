#include "MyDPSActors.h"
#include "MQMyDPS.h"

#include "Proto/mydps.pb.h"

#include <mq/Plugin.h>
#include "mq/base/String.h"

#include <algorithm>
#include <cctype>

namespace
{
const char* kMailbox = "mydps";
constexpr auto kLiveInterval    = std::chrono::seconds(1);
constexpr auto kSessionInterval = std::chrono::seconds(3);
constexpr auto kPruneInterval   = std::chrono::seconds(5);
constexpr auto kStaleAfter      = std::chrono::seconds(60);
constexpr size_t kMaxPeerBattles = 50;

std::vector<std::string> GetLocalGroupNames()
{
	std::vector<std::string> names;
	if (!pLocalPC || !pLocalPC->Group)
	{
		return names;
	}

	for (uint32_t i = 0; i < MAX_GROUP_SIZE; ++i)
	{
		CGroupMember* pMember = pLocalPC->Group->GetGroupMember(static_cast<int>(i));
		if (pMember && pMember->GetName() && pMember->GetName()[0] != '\0')
		{
			names.push_back(pMember->GetName());
		}
	}
	return names;
}

std::vector<std::string> GetLocalRaidNames()
{
	std::vector<std::string> names;
	if (!pRaid || pRaid->RaidMemberCount <= 0)
	{
		return names;
	}

	auto& raidMembers = pRaid->RaidMember;
	for (int i = 0; i < MAX_RAID_SIZE; ++i)
	{
		const RaidMember& rm = raidMembers[i];
		if (rm.Name[0] != '\0')
		{
			names.push_back(rm.Name);
		}
	}
	return names;
}

bool ContainsName(const std::vector<std::string>& names, const std::string& name)
{
	for (const std::string& n : names)
	{
		if (ci_equals(n, name))
		{
			return true;
		}
	}
	return false;
}
} // namespace

void MyDPSActors::Log(const std::string& msg) const
{
	if (m_engine)
	{
		m_engine->Output(msg);
	}
	else
	{
		WriteChatf("%s", msg.c_str());
	}
}

void MyDPSActors::LogDebug(const std::string& msg) const
{
	if (m_engine)
	{
		m_engine->DebugOutput(msg);
	}
}

std::string PeerKey(const std::string& server, const std::string& character)
{
	return server + "|" + character;
}

ImVec4 PeerColorForName(const std::string& name)
{
	static const ImVec4 palette[] = {
		ImVec4(0.91f, 0.30f, 0.24f, 1.0f),
		ImVec4(0.18f, 0.80f, 0.44f, 1.0f),
		ImVec4(0.20f, 0.60f, 0.86f, 1.0f),
		ImVec4(0.95f, 0.77f, 0.06f, 1.0f),
		ImVec4(0.61f, 0.35f, 0.71f, 1.0f),
		ImVec4(0.90f, 0.49f, 0.13f, 1.0f),
		ImVec4(0.10f, 0.74f, 0.61f, 1.0f),
		ImVec4(0.93f, 0.51f, 0.93f, 1.0f),
		ImVec4(0.60f, 0.80f, 0.20f, 1.0f),
		ImVec4(0.40f, 0.55f, 0.95f, 1.0f),
		ImVec4(0.85f, 0.65f, 0.13f, 1.0f),
		ImVec4(0.55f, 0.76f, 0.29f, 1.0f),
	};
	constexpr int count = sizeof(palette) / sizeof(palette[0]);

	uint32_t hash = 2166136261u;
	for (char c : name)
	{
		hash ^= static_cast<uint8_t>(std::tolower(static_cast<unsigned char>(c)));
		hash *= 16777619u;
	}
	return palette[hash % count];
}

void MyDPSActors::Init(MyDPSEngine* engine, const std::string& server, const std::string& character)
{
	m_engine = engine;
	m_server = server;
	m_character = character;
	m_peers.clear();

	if (m_dropbox.Dropbox)
	{
		m_dropbox.Remove();
	}

	m_dropbox = mq::postoffice::AddActor(kMailbox,
		[this](const std::shared_ptr<mq::postoffice::Message>& msg) { OnReceive(msg); });

	m_valid = m_dropbox.Dropbox != nullptr;

	m_selfRecord = PeerRecord{};
	m_selfRecord.server = m_server;
	m_selfRecord.character = m_character;
	m_selfRecord.isSelf = true;
	m_selfRecord.color = PeerColorForName(m_character);

	auto now = std::chrono::steady_clock::now();
	m_nextLive = now;
	m_nextSession = now;
	m_nextPrune = now + kPruneInterval;
}

void MyDPSActors::Shutdown()
{
	if (m_dropbox.Dropbox)
	{
		m_dropbox.Remove();
	}
	m_dropbox = mq::postoffice::DropboxAPI{};
	m_valid = false;
	m_peers.clear();
}

void MyDPSActors::Pulse()
{
	if (!m_valid || !m_engine || !pLocalPC)
	{
		return;
	}

	auto now = std::chrono::steady_clock::now();

	if (now >= m_nextLive)
	{
		PublishLive();
		m_nextLive = now + kLiveInterval;
	}
	if (now >= m_nextSession)
	{
		PublishSession();
		m_nextSession = now + kSessionInterval;
	}
	if (now >= m_nextPrune)
	{
		PruneStale();
		m_nextPrune = now + kPruneInterval;
	}
}

void MyDPSActors::PublishLive()
{
	mq::proto::mydps::MyDPSEnvelope env;
	auto* l = env.mutable_live();
	l->set_server(m_server);
	l->set_character(m_character);
	if (pLocalPlayer)
	{
		if (const char* cls = pLocalPlayer->GetClassThreeLetterCode())
		{
			l->set_class_short(cls);
		}
	}
	l->set_in_combat(m_engine->inCombat);
	l->set_dps(m_engine->GetBattleDPS());
	l->set_total_damage(m_engine->battleDamage);
	l->set_hit_count(m_engine->battleHitCount);
	l->set_crit_damage(m_engine->battleCritDamage);
	l->set_dot_damage(m_engine->battleDotDamage);
	l->set_pet_damage(m_engine->battlePetDamage);
	l->set_non_melee_damage(m_engine->battleNonMeleeDmg);
	l->set_ds_damage(m_engine->battleDsDamage);
	l->set_direct_heals(m_engine->battleDirectHeals);
	l->set_crit_heals(m_engine->battleCritHeals);
	l->set_duration_seconds(m_engine->GetBattleDuration());

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
	++m_sentCount;
}

void MyDPSActors::PublishSession()
{
	int64_t sessionHeals = 0;
	for (const auto& [name, ht] : m_engine->cachedSessionHeals)
		sessionHeals += ht.GetTotalHeals();

	mq::proto::mydps::MyDPSEnvelope env;
	auto* s = env.mutable_session();
	s->set_server(m_server);
	s->set_character(m_character);
	if (pLocalPlayer)
	{
		if (const char* cls = pLocalPlayer->GetClassThreeLetterCode())
		{
			s->set_class_short(cls);
		}
	}
	s->set_total_damage(m_engine->sessionDamage);
	s->set_total_heals(sessionHeals);
	s->set_session_dps(m_engine->GetSessionDPS());
	s->set_battle_count(static_cast<int>(m_engine->battleHistory.size()));

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
	++m_sentCount;
}

void MyDPSActors::PublishFinalizedBattle(const BattleData& battle)
{
	if (!m_valid)
	{
		return;
	}

	mq::proto::mydps::MyDPSEnvelope env;
	auto* b = env.mutable_battle();
	b->set_server(m_server);
	b->set_character(m_character);
	b->set_battle_number(battle.battleNumber);
	b->set_total_damage(battle.totalDamage);
	b->set_crit_damage(battle.critDamage);
	b->set_dot_damage(battle.dotDamage);
	b->set_pet_damage(battle.petDamage);
	b->set_non_melee_damage(battle.nonMeleeDamage);
	b->set_ds_damage(battle.dsDamage);
	b->set_direct_heals(battle.directHeals);
	b->set_crit_heals(battle.critHeals);
	b->set_hit_count(battle.hitCount);
	b->set_duration_seconds(battle.durationSeconds);
	b->set_dps(battle.dps);
	b->set_avg_damage(battle.avgDamage);
	b->set_start_time_ms(battle.startTimeMs);

	mq::postoffice::Address addr;
	addr.Mailbox = kMailbox;
	m_dropbox.Post(addr, env);
}

void MyDPSActors::OnReceive(const std::shared_ptr<mq::postoffice::Message>& msg)
{
	++m_recvCount;

	if (!msg || !msg->Payload)
	{
		return;
	}

	mq::proto::mydps::MyDPSEnvelope env;
	if (!env.ParseFromString(*msg->Payload))
	{
		++m_parseFailCount;
		return;
	}

	auto now = std::chrono::steady_clock::now();

	switch (env.payload_case())
	{
	case mq::proto::mydps::MyDPSEnvelope::PayloadCase::kLive:
	{
		const auto& l = env.live();
		if (ci_equals(l.character(), m_character))
		{
			++m_selfIgnoredCount;
			return;
		}

		auto& p = m_peers[PeerKey(l.server(), l.character())];
		bool isNew = p.character.empty();
		if (isNew)
		{
			p.color = PeerColorForName(l.character());
		}
		p.server        = l.server();
		p.character     = l.character();
		if (!l.class_short().empty())
		{
			p.classShort = l.class_short();
		}
		p.inCombat      = l.in_combat();
		p.liveDPS       = l.dps();
		p.liveTotal     = l.total_damage();
		p.liveHits      = l.hit_count();
		p.liveCrit      = l.crit_damage();
		p.liveDot       = l.dot_damage();
		p.livePet       = l.pet_damage();
		p.liveNonMelee  = l.non_melee_damage();
		p.liveDS        = l.ds_damage();
		p.liveHeals     = l.direct_heals();
		p.liveCritHeals = l.crit_heals();
		p.liveDuration  = l.duration_seconds();
		p.lastSeen      = now;
		if (isNew)
		{
			LogDebug(fmt::format("\at[MQMyDPS]\ax Discovered peer \ay{}\ax ({}).", p.character, p.server));
		}
		break;
	}
	case mq::proto::mydps::MyDPSEnvelope::PayloadCase::kSession:
	{
		const auto& s = env.session();
		if (ci_equals(s.character(), m_character))
		{
			++m_selfIgnoredCount;
			return;
		}

		auto& p = m_peers[PeerKey(s.server(), s.character())];
		bool isNew = p.character.empty();
		if (isNew)
		{
			p.color = PeerColorForName(s.character());
		}
		p.server        = s.server();
		p.character     = s.character();
		if (!s.class_short().empty())
		{
			p.classShort = s.class_short();
		}
		p.sessionDamage = s.total_damage();
		p.sessionHeals  = s.total_heals();
		p.sessionDPS    = s.session_dps();
		p.battleCount   = s.battle_count();
		p.lastSeen      = now;
		if (isNew)
		{
			LogDebug(fmt::format("\at[MQMyDPS]\ax Discovered peer \ay{}\ax ({}).", p.character, p.server));
		}
		break;
	}
	case mq::proto::mydps::MyDPSEnvelope::PayloadCase::kBattle:
	{
		const auto& b = env.battle();
		if (ci_equals(b.character(), m_character))
		{
			++m_selfIgnoredCount;
			return;
		}

		auto& p = m_peers[PeerKey(b.server(), b.character())];
		bool isNew = p.character.empty();
		if (isNew)
		{
			p.color = PeerColorForName(b.character());
		}
		p.server    = b.server();
		p.character = b.character();
		p.lastSeen  = now;
		if (isNew)
		{
			LogDebug(fmt::format("\at[MQMyDPS]\ax Discovered peer \ay{}\ax ({}).", p.character, p.server));
		}

		PeerBattle pb;
		pb.battleNumber    = b.battle_number();
		pb.totalDamage     = b.total_damage();
		pb.critDamage      = b.crit_damage();
		pb.dotDamage       = b.dot_damage();
		pb.petDamage       = b.pet_damage();
		pb.nonMeleeDamage  = b.non_melee_damage();
		pb.dsDamage        = b.ds_damage();
		pb.directHeals     = b.direct_heals();
		pb.critHeals       = b.crit_heals();
		pb.hitCount        = b.hit_count();
		pb.durationSeconds = b.duration_seconds();
		pb.dps             = b.dps();
		pb.avgDamage       = b.avg_damage();
		pb.startTimeMs     = b.start_time_ms();
		p.battles.push_back(pb);
		while (p.battles.size() > kMaxPeerBattles)
		{
			p.battles.pop_front();
		}
		break;
	}
	default:
		break;
	}
}

void MyDPSActors::DebugDump() const
{
	Log(fmt::format("\at[MQMyDPS]\ax Actors: valid=\ay{}\ax mailbox='mydps' self=\ag{}|{}\ax peers=\ay{}\ax",
		m_valid ? "yes" : "no", m_server, m_character, static_cast<int>(m_peers.size())));
	Log(fmt::format("  sent=\aw{}\ax recv=\aw{}\ax parseFail=\ar{}\ax selfIgnored=\aw{}\ax",
		m_sentCount, m_recvCount, m_parseFailCount, m_selfIgnoredCount));

	auto now = std::chrono::steady_clock::now();
	for (const auto& [key, p] : m_peers)
	{
		auto age = std::chrono::duration_cast<std::chrono::seconds>(now - p.lastSeen).count();
		Log(fmt::format("  \ay{}\ax liveDPS=\aw{:.1f}\ax sessDmg=\ag{}\ax battles=\aw{}\ax seen=\aw{}s\ax ago",
			key, p.liveDPS, FormatNumber(p.sessionDamage),
			static_cast<int>(p.battles.size()), static_cast<long long>(age)));
	}
}

void MyDPSActors::PruneStale()
{
	auto now = std::chrono::steady_clock::now();
	for (auto it = m_peers.begin(); it != m_peers.end();)
	{
		if (now - it->second.lastSeen > kStaleAfter)
		{
			it = m_peers.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void MyDPSActors::BuildSelfRecord()
{
	if (!m_engine)
	{
		return;
	}

	m_selfRecord.server = m_server;
	m_selfRecord.character = m_character;
	m_selfRecord.isSelf = true;
	if (m_selfRecord.color.x == 1.0f && m_selfRecord.color.y == 1.0f && m_selfRecord.color.z == 1.0f)
	{
		m_selfRecord.color = PeerColorForName(m_character);
	}
	if (pLocalPlayer)
	{
		if (const char* cls = pLocalPlayer->GetClassThreeLetterCode())
		{
			m_selfRecord.classShort = cls;
		}
	}

	m_selfRecord.inCombat      = m_engine->inCombat;
	m_selfRecord.liveDPS       = m_engine->GetBattleDPS();
	m_selfRecord.liveTotal     = m_engine->battleDamage;
	m_selfRecord.liveHits      = m_engine->battleHitCount;
	m_selfRecord.liveCrit      = m_engine->battleCritDamage;
	m_selfRecord.liveDot       = m_engine->battleDotDamage;
	m_selfRecord.livePet       = m_engine->battlePetDamage;
	m_selfRecord.liveNonMelee  = m_engine->battleNonMeleeDmg;
	m_selfRecord.liveDS        = m_engine->battleDsDamage;
	m_selfRecord.liveHeals     = m_engine->battleDirectHeals;
	m_selfRecord.liveCritHeals = m_engine->battleCritHeals;
	m_selfRecord.liveDuration  = m_engine->GetBattleDuration();

	int64_t sessionHeals = 0;
	for (const auto& [name, ht] : m_engine->cachedSessionHeals)
		sessionHeals += ht.GetTotalHeals();

	m_selfRecord.sessionDamage = m_engine->sessionDamage;
	m_selfRecord.sessionHeals  = sessionHeals;
	m_selfRecord.sessionDPS    = m_engine->GetSessionDPS();
	m_selfRecord.battleCount   = static_cast<int>(m_engine->battleHistory.size());

	m_selfRecord.battles.clear();
	size_t total = m_engine->battleHistory.size();
	size_t start = total > kMaxPeerBattles ? total - kMaxPeerBattles : 0;
	for (size_t i = start; i < total; ++i)
	{
		const BattleData& bd = m_engine->battleHistory[i];
		PeerBattle pb;
		pb.battleNumber    = bd.battleNumber;
		pb.totalDamage     = bd.totalDamage;
		pb.critDamage      = bd.critDamage;
		pb.dotDamage       = bd.dotDamage;
		pb.petDamage       = bd.petDamage;
		pb.nonMeleeDamage  = bd.nonMeleeDamage;
		pb.dsDamage        = bd.dsDamage;
		pb.directHeals     = bd.directHeals;
		pb.critHeals       = bd.critHeals;
		pb.hitCount        = bd.hitCount;
		pb.durationSeconds = bd.durationSeconds;
		pb.dps             = bd.dps;
		pb.avgDamage       = bd.avgDamage;
		pb.startTimeMs     = bd.startTimeMs;
		m_selfRecord.battles.push_back(pb);
	}
}

std::vector<const PeerRecord*> MyDPSActors::VisiblePeers(PeerFilterMode mode)
{
	BuildSelfRecord();

	std::vector<std::string> roster;
	if (mode == PeerFilterMode::SameGroup)
	{
		roster = GetLocalGroupNames();
	}
	else if (mode == PeerFilterMode::SameRaid)
	{
		roster = GetLocalRaidNames();
	}

	std::vector<const PeerRecord*> result;
	result.push_back(&m_selfRecord);

	for (const auto& [key, peer] : m_peers)
	{
		bool include = false;
		switch (mode)
		{
		case PeerFilterMode::All:
			include = true;
			break;
		case PeerFilterMode::SameServer:
			include = ci_equals(peer.server, m_server);
			break;
		case PeerFilterMode::SameGroup:
		case PeerFilterMode::SameRaid:
			include = ci_equals(peer.server, m_server) && ContainsName(roster, peer.character);
			break;
		}

		if (include)
		{
			result.push_back(&peer);
		}
	}

	std::sort(result.begin(), result.end(),
		[](const PeerRecord* a, const PeerRecord* b) { return a->liveDPS > b->liveDPS; });

	return result;
}
