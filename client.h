#ifndef CLIENT_H_
#define CLIENT_H_

#include "nss.h"
#include "pk11pub.h"
#include "ssl.h"

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

}  // namespace mumble_message


class MumbleClient {
  public:
	void Connect();
	void sendMessage(PRFileDesc* fd, PbMessageType::MessageType type, const ::google::protobuf::Message& msg, bool print);

  private:
	friend class MumbleClientLib;
	MumbleClient();

	void ParseMessage(const mumble_message::MessageHeader& msg_header, void* buffer);
	static void SSLHandshakeCallback(PRFileDesc* , void*);
	static SECStatus SSLBadCertificateCallback(void*, PRFileDesc*);

	MumbleClient(const MumbleClient&);
	void operator=(const MumbleClient&);
};

#endif  // CLIENT_H_
