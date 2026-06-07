#pragma once

#include <mq/Plugin.h>

#include <string_view>

namespace mqmychat
{

constexpr int MQMYCHAT_API_VERSION = 1;

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
		return Send(channel, message, MQColor(240, 240, 240));
	}
};

inline ChatAPI* GetChatAPI()
{
	return static_cast<ChatAPI*>(mq::GetPluginInterface("MQMyChat"));
}

} // namespace mqmychat
