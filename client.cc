#include "client.h"

#include "prmem.h"
#include "prnetdb.h"

#include <deque>
#include <iostream>
#include <typeinfo>

#include "settings.h"

using mumble_message::MessageHeader;
using mumble_message::Message;

///////////////////////////////////////////////////////////////////////////////

#define MUMBLE_VERSION(x, y, z) ((x << 16) | (y << 8) | (z & 0xFF))

namespace {

template <class T> T ConstructProtobufObject(void* buffer, int32 length, bool print) {
	T pb;
	pb.ParseFromArray(buffer, length);
	if (print) {
		std::cout << ">> IN: " << typeid(T).name() << ":" << std::endl;
		pb.PrintDebugString();
	}
	return pb;
}

}  // namespace


///////////////////////////////////////////////////////////////////////////////
// MumbleClient, private:

MumbleClient::MumbleClient() : state_(kStateNew) {
}

// static
void MumbleClient::SSLHandshakeCallback(PRFileDesc* /* fd */, void* client_data) {
	MumbleClient* mc = static_cast<MumbleClient *>(client_data);
	std::cout << "Handshake completed" << std::endl;
	mc->state_ = kStateHandshakeCompleted;
}

// static
SECStatus MumbleClient::SSLBadCertificateCallback(void* /* arg */, PRFileDesc* /* fd */) {
	std::cout << "Cert check failed" << std::endl;
	return SECSuccess;
}

void MumbleClient::ParseMessage(const MessageHeader& msg_header, void* buffer) {
	switch (msg_header.type) {
	case PbMessageType::Version: {
		MumbleProto::Version v = ConstructProtobufObject<MumbleProto::Version>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::Ping: {
		MumbleProto::Ping p = ConstructProtobufObject<MumbleProto::Ping>(buffer, msg_header.length, false);
		break;
	}
	case PbMessageType::CryptSetup: {
		MumbleProto::CryptSetup cs = ConstructProtobufObject<MumbleProto::CryptSetup>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::CodecVersion: {
		MumbleProto::CodecVersion cv = ConstructProtobufObject<MumbleProto::CodecVersion>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::ServerSync: {
		MumbleProto::ServerSync ss = ConstructProtobufObject<MumbleProto::ServerSync>(buffer, msg_header.length, true);
		state_ = kStateAuthenticated;
		break;
	}
	default:
		std::cout << ">> IN: Unhandled message - Type: " << msg_header.type << " Length: " << msg_header.length << std::endl;
	}
}

bool MumbleClient::ProcessTCPSendQueue() {
	while (!send_queue_.empty()) {
		Message msg = send_queue_.front();

		PRIOVec iv[2];
		iv[0].iov_base = reinterpret_cast<char *>(&msg.header_);
		iv[0].iov_len = sizeof(msg.header_);
		iv[1].iov_base = const_cast<char *>(msg.msg_.data());
		iv[1].iov_len = msg.msg_.size();

		int32 ret = PR_Writev(tcp_socket_, reinterpret_cast<PRIOVec *>(&iv), 2, PR_INTERVAL_NO_TIMEOUT);
		if (ret == -1) {
			std::cerr << "DEQUEUE: Write would block..." << std::endl;
			return true;
		}
		std::cout << "<< DEQUEUE: Type: " << ntohs(msg.header_.type) << " Length: 6+" << msg.msg_.size() << std::endl;
		send_queue_.pop_front();
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// MumbleClient, public:

void MumbleClient::Connect() {
	// Resolve hostname
	std::cerr << "Resolving host " << Settings::getHost() << std::endl;

	char buf[PR_NETDB_BUF_SIZE];
	PRHostEnt host_entry;
	if (PR_GetHostByName(Settings::getHost().c_str(), buf, sizeof(buf), &host_entry) == PR_FAILURE) {
		std::cout << "unknown host name: " << Settings::getHost() << std::endl;
		exit(1);
	}

	PRNetAddr addr;
	/*PRIntn er =*/ PR_EnumerateHostEnt(0, &host_entry, Settings::getPort(), &addr);

	udp_socket_ = PR_NewUDPSocket();
	tcp_socket_ = PR_NewTCPSocket();

	// Connect
	std::cerr << "Connecting..." << std::endl;
	if (PR_Connect(tcp_socket_, &addr, PR_SecondsToInterval(30)) != PR_SUCCESS) {
		std::cerr << "Connection failed " << PR_GetError() << std::endl;
		exit(1);
	}
	PR_Connect(udp_socket_, &addr, PR_INTERVAL_NO_TIMEOUT);

	// Set SSL options
	SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_FALSE);
	SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_FALSE);
	SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
	SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);

	tcp_socket_ = SSL_ImportFD(NULL, tcp_socket_);
	SSL_HandshakeCallback(tcp_socket_, SSLHandshakeCallback, this);
	SSL_BadCertHook(tcp_socket_, SSLBadCertificateCallback, NULL);
	SSL_SetURL(tcp_socket_, Settings::getHost().c_str());
	PRSocketOptionData po = { PR_SockOpt_Nonblocking, {PR_TRUE} };
	PR_SetSocketOption(tcp_socket_, &po);

	// Force SSL handshake
	SSL_ResetHandshake(tcp_socket_, PR_FALSE);
	SSL_ForceHandshake(tcp_socket_);

	// Send initial messages
	MumbleProto::Version v;
	v.set_version(MUMBLE_VERSION(1, 2, 2));
	v.set_release("0.0.1-dev");
	sendMessage(PbMessageType::Version, v, true);

	MumbleProto::Authenticate a;
	a.set_username(Settings::getUserName());
	a.set_password(Settings::getPassword());
	// FIXME: hardcoded version number
	a.add_celt_versions(0x8000000b);
	sendMessage(PbMessageType::Authenticate, a, true);

	// Net event loop
	PRPollDesc pds[2];
	pds[0].fd = tcp_socket_;
	pds[0].in_flags = PR_POLL_READ | PR_POLL_WRITE;
	pds[1].fd = udp_socket_;
	pds[1].in_flags = PR_POLL_READ; // | PR_POLL_WRITE;

	PRIntervalTime ping_interval = PR_SecondsToInterval(5);
	PRIntervalTime ping_timer = PR_IntervalNow();

	while (PR_TRUE) {
		int32 ret = PR_Poll(pds, 2, PR_SecondsToInterval(1));
		if (ret == -1) {
			std::cerr << "PR_Poll failed" << std::endl;
			exit(1);
		}

		// Ping handling
		if (state_ >= kStateAuthenticated && static_cast<PRIntervalTime>(PR_IntervalNow() - ping_timer) > ping_interval) {
			ping_timer = PR_IntervalNow();
			MumbleProto::Ping p;
			p.set_timestamp(PR_Now());
			sendMessage(PbMessageType::Ping, p, false);

			if (ret == 0)
				continue;
		}

		for (int i = 0; i < 2; ++i) {
			// Check if we want to write data
			if (pds[i].fd == tcp_socket_) {
				if (!send_queue_.empty()) {
					pds[i].in_flags = pds[i].in_flags | PR_POLL_WRITE;
				} else {
					pds[i].in_flags = pds[i].in_flags ^ PR_POLL_WRITE;
				}
			}

			// TCP socket handling - write
			if (pds[i].fd == tcp_socket_ && pds[i].out_flags & PR_POLL_WRITE) {
				if (state_ >= kStateHandshakeCompleted)
					ProcessTCPSendQueue();
			}
			// TCP socket handling - read
			if (pds[i].fd == tcp_socket_ &&pds[i].out_flags & PR_POLL_READ) {
				while (PR_TRUE) {
					// Receive message header
					MessageHeader msg_header;
					ret = PR_Recv(pds[i].fd, &msg_header, sizeof(msg_header), 0, PR_INTERVAL_NO_TIMEOUT);
					if (ret == -1) {
						if (PR_GetError() == PR_WOULD_BLOCK_ERROR)
							break;
						std::cerr << "PR_Recv failed" << std::endl;
						exit(1);
					}

					msg_header.type = PR_ntohs(msg_header.type);
					msg_header.length = PR_ntohl(msg_header.length);

					if (msg_header.length >= 0x7FFFF)
						exit(1);

					// Receive message body
					char* buffer = static_cast<char *>(PR_Malloc(msg_header.length));
					ret = PR_Recv(pds[i].fd, buffer, msg_header.length, 0, PR_INTERVAL_NO_TIMEOUT);

					ParseMessage(msg_header, buffer);
					PR_Free(buffer);
				}
			} else if (pds[i].out_flags & PR_POLL_ERR) {
				std::cout << "PR_Poll: fd error" << std::endl;
				exit(1);
			} else if (pds[i].out_flags & PR_POLL_NVAL) {
				std::cout << "PR_Poll: fd invalid" << std::endl;
				exit(1);
			}

			// UDP socket handling - write
			if(pds[i].fd == udp_socket_ && pds[i].out_flags & PR_POLL_WRITE) {
				// NOT IMPLEMENTED
			}
			// UDP socket handling - read
			if (pds[i].fd == udp_socket_ &&pds[i].out_flags & PR_POLL_READ) {
				// NOT IMPLEMENTED
			} else if (pds[i].out_flags & PR_POLL_ERR) {
				std::cout << "PR_Poll: fd error" << std::endl;
				exit(1);
			} else if (pds[i].out_flags & PR_POLL_NVAL) {
				std::cout << "PR_Poll: fd invalid" << std::endl;
				exit(1);
			}
		}
	}
}

void MumbleClient::sendMessage(PbMessageType::MessageType type, const ::google::protobuf::Message& msg, bool print) {
	if (print) {
		std::cout << "<< ENQUEUE: " << type << std::endl;
		msg.PrintDebugString();
	}

	int32 length = msg.ByteSize();
	MessageHeader msg_header;
	msg_header.type = PR_htons(static_cast<int16>(type));
	msg_header.length = PR_htonl(length);

	std::string pb_message = msg.SerializeAsString();

	Message message(msg_header, pb_message);
	send_queue_.push_back(message);
}
