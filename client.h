#ifndef CLIENT_H_
#define CLIENT_H_

#include "stdint.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/ptr_container/ptr_deque.hpp>
#include <boost/shared_ptr.hpp>

#include "messages.h"
#include "Mumble.pb.h"

namespace MumbleClient {

using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using boost::asio::ssl::stream;

#define SSL 1

class Channel;
class CryptState;
class Settings;
class User;

namespace mumble_message {

#pragma pack(push)
#pragma pack(1)
struct MessageHeader {
	int16_t type;
	int32_t length;
} /*__attribute__((packed))*/;
#pragma pack(pop)

struct Message {
	MessageHeader header_;
	std::string msg_;

	Message(MessageHeader& header, std::string& msg) : header_(header), msg_(msg) {};
};

}  // namespace mumble_message

typedef std::list< boost::shared_ptr<User> >::iterator user_list_iterator;
typedef std::list< boost::shared_ptr<Channel> >::iterator channel_list_iterator;

typedef boost::function<void (const std::string& text)> TextMessageCallbackType;
typedef boost::function<void ()> AuthCallbackType;
typedef boost::function<void (int32_t length, void* buffer)> RawUdpTunnelCallbackType;
typedef boost::function<void (const User& user)> UserJoinedCallbackType;
typedef boost::function<void (const User& user)> UserLeftCallbackType;
typedef boost::function<void (const User& user, const Channel& channel)> UserMovedCallbackType;
typedef boost::function<void (const Channel& channel)> ChannelAddCallbackType;
typedef boost::function<void (const Channel& channel)> ChannelRemoveCallbackType;

class MumbleClient {
	enum State {
		kStateNew,
		kStateHandshakeCompleted,
		kStateAuthenticated
	};

  public:
	~MumbleClient();
	void Connect(const Settings& s);
	void Disconnect();
	void SendMessage(PbMessageType::MessageType type, const ::google::protobuf::Message& msg, bool print);
	void SetComment(const std::string& text);
	void SendRawUdpTunnel(const char* buffer, int32_t len);
	void SendUdpMessage(const char* buffer, int32_t len);

	void SetTextMessageCallback(TextMessageCallbackType tm) { text_message_callback_ = tm; }
	void SetAuthCallback(AuthCallbackType a) { auth_callback_ = a; }
	void SetRawUdpTunnelCallback(RawUdpTunnelCallbackType rut) { raw_udp_tunnel_callback_ = rut; }
	void SetUserJoinedCallback(UserJoinedCallbackType ujt) { user_joined_callback_ = ujt; }
	void SetUserLeftCallback(UserJoinedCallbackType ult) { user_left_callback_ = ult; }
	void SetUserMovedCallback(UserMovedCallbackType umt) { user_moved_callback_ = umt; }
	void SetChannelAddCallback(ChannelAddCallbackType cat) { channel_add_callback_ = cat; }
	void SetChannelRemoveCallback(ChannelRemoveCallbackType crt) { channel_remove_callback_ = crt; }

#ifndef NDEBUG
	void PrintChannelList();
	void PrintUserList();
#endif

  private:
	friend class MumbleClientLib;
	MumbleClient(boost::asio::io_service* io_service);

	void DoPing(const boost::system::error_code& error);
	void ParseMessage(const mumble_message::MessageHeader& msg_header, void* buffer);
	void ProcessTCPSendQueue(const boost::system::error_code& error, const size_t bytes_transferred);
	void SendFirstQueued();
	void ReadHandler(const boost::system::error_code& error);
	void HandleUserRemove(const MumbleProto::UserRemove& ur);
	void HandleUserState(const MumbleProto::UserState& us);
	void HandleChannelState(const MumbleProto::ChannelState& cs);
	void HandleChannelRemove(const MumbleProto::ChannelRemove& cr);
	boost::shared_ptr<User> FindUser(int32_t session);
	boost::shared_ptr<Channel> FindChannel(int32_t id);

	boost::asio::io_service* io_service_;
#if SSL
	stream<tcp::socket>* tcp_socket_;
#else
	tcp::socket* tcp_socket_;
#endif
	udp::socket* udp_socket_;
	CryptState* cs_;
	boost::ptr_deque<mumble_message::Message> send_queue_;
	State state_;
	boost::asio::deadline_timer* ping_timer_;
	int32_t session_;
	std::list< boost::shared_ptr<User> > user_list_;
	std::list< boost::shared_ptr<Channel> > channel_list_;

	TextMessageCallbackType text_message_callback_;
	AuthCallbackType auth_callback_;
	RawUdpTunnelCallbackType raw_udp_tunnel_callback_;
	UserJoinedCallbackType user_joined_callback_;
	UserLeftCallbackType user_left_callback_;
	UserMovedCallbackType user_moved_callback_;
	ChannelAddCallbackType channel_add_callback_;
	ChannelRemoveCallbackType channel_remove_callback_;

	MumbleClient(const MumbleClient&);
	void operator=(const MumbleClient&);
};

}  // end namespace MumbleClient

#endif  // CLIENT_H_
