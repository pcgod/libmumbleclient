#ifndef CLIENT_H_
#define CLIENT_H_

#include "nss.h"
#include "pk11pub.h"
#include "ssl.h"

#include <deque>

#include "messages.h"
#include "Mumble.pb.h"

namespace mumble_message {

#pragma pack(push)
#pragma pack(1)
struct MessageHeader {
	int16 type;
	int32 length;
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
	void Connect();
	void sendMessage(PbMessageType::MessageType type, const ::google::protobuf::Message& msg, bool print);

  private:
	friend class MumbleClientLib;
	MumbleClient();

	void ParseMessage(const mumble_message::MessageHeader& msg_header, void* buffer);
	bool ProcessTCPSendQueue();
	static void SSLHandshakeCallback(PRFileDesc*, void*);
	static SECStatus SSLBadCertificateCallback(void*, PRFileDesc*);

	std::deque<mumble_message::Message> send_queue_;
	State state_;
	PRFileDesc* tcp_socket_;
	PRFileDesc* udp_socket_;

	MumbleClient(const MumbleClient&);
	void operator=(const MumbleClient&);
};

#endif  // CLIENT_H_
