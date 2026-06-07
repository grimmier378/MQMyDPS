//
// MyChatAPI.h
//
// Vendored from grimmier378/MQMyChat (MyChatAPI.h). MQMyDPS only needs this header
// to talk to a loaded MQMyChat via GetPluginInterface("MQMyChat"); there is no link
// dependency. Do NOT edit by hand -- refresh with Update-MyChatAPI.ps1.
//
// Upstream: https://github.com/grimmier378/MQMyChat (branch main)
//
#pragma once

#include <mq/Plugin.h>

#include <string_view>

namespace mqmychat
{

constexpr int MQMYCHAT_API_VERSION = 1;

constexpr MQColor MQMYCHAT_DEFAULT_COLOR{ 240, 240, 240 };

class ChatAPI : public mq::PluginInterface
{
public:
	virtual int GetAPIVersion() const = 0;

	virtual bool Send(std::string_view channel, std::string_view message, MQColor color) = 0;
	virtual void SendToMain(std::string_view message, MQColor color) = 0;

	virtual bool ChannelExists(std::string_view channel) const = 0;
	virtual int  GetChannelCount() const = 0;
	virtual bool IsChannelEnabled(std::string_view channel) const = 0;
	virtual bool CreateChannel(std::string_view channel) = 0;
	virtual void Clear(std::string_view channel) = 0;

	bool Send(std::string_view channel, std::string_view message)
	{
		return Send(channel, message, MQMYCHAT_DEFAULT_COLOR);
	}
};

inline ChatAPI* GetChatAPI()
{
	return static_cast<ChatAPI*>(mq::GetPluginInterface("MQMyChat"));
}

} // namespace mqmychat
