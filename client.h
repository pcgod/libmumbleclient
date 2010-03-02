#ifndef CLIENT_H_
#define CLIENT_H_

#include "stdint.h"

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <deque>

#include "messages.h"
#include "Mumble.pb.h"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using boost::asio::ssl::stream;

#define SSL 1

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


class MumbleClient {
	enum State {
		kStateNew,
		kStateHandshakeCompleted,
		kStateAuthenticated
	};

  public:
	~MumbleClient();
	void Connect();
	void sendMessage(PbMessageType::MessageType type, const ::google::protobuf::Message& msg, bool print);

  private:
	friend class MumbleClientLib;
	MumbleClient();

	void DoPing(const boost::system::error_code& error);
	void ParseMessage(const mumble_message::MessageHeader& msg_header, void* buffer);
	void ProcessTCPSendQueue(const boost::system::error_code& error, const size_t bytes_transferred);
	void ReadWriteHandler(const boost::system::error_code& error);

	boost::asio::io_service io_service_;
	std::deque<mumble_message::Message> send_queue_;
	State state_;
#if SSL
	stream<tcp::socket>* tcp_socket_;
#else
	tcp::socket* tcp_socket_;
#endif
	udp::socket* udp_socket_;
	boost::asio::deadline_timer* ping_timer_;

	MumbleClient(const MumbleClient&);
	void operator=(const MumbleClient&);
};

#endif  // CLIENT_H_
