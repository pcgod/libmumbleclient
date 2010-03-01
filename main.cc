#include <iostream>
#include <stdint.h>
#include <typeinfo>

#include "prmem.h"
#include "prnetdb.h"

#include "nss.h"
#include "pk11pub.h"
#include "ssl.h"

#include "messages.h"
#include "Mumble.pb.h"
#include "settings.h"

#define MUMBLE_VERSION(x, y, z) ((x << 16) | (y << 8) | (z & 0xFF))

#pragma pack(push)
#pragma pack(1)
struct MessageHeader {
	int16 type;
	int32 length;
} /*__attribute__((packed))*/;
#pragma pack(pop)

void sendMessage(PRFileDesc *fd, PbMessageType::MessageType type, ::google::protobuf::Message &msg, bool print) {
	if (print) {
		std::cout << "<< " << type << std::endl;
		msg.PrintDebugString();
	}

	int32 length = msg.ByteSize();

/*
	char msgHeader[MSG_HEADER_LEN];
	*((int16 *)msgHeader) = PR_htons(static_cast<int16>(type));
	*((int32 *)(msgHeader + 2)) = PR_htonl(len);
	PR_Send(fd, msgHeader, MSG_HEADER_LEN, NULL, PR_INTERVAL_NO_TIMEOUT);
*/

	MessageHeader msg_header;
	msg_header.type = PR_htons(static_cast<int16>(type));
	msg_header.length = PR_htonl(length);
	PR_Send(fd, &msg_header, sizeof(msg_header), NULL, PR_INTERVAL_NO_TIMEOUT);

	std::string pbMessage;
	msg.SerializeToString(&pbMessage);
	PR_Send(fd, pbMessage.c_str(), length, NULL, PR_INTERVAL_NO_TIMEOUT);
}

template <class T> T ConstructProtobufObject(void *buffer, int32 length, bool print) {
	T pb;
	pb.ParseFromArray(buffer, length);
	if (print) {
		std::cout << ">> " << typeid(T).name() << ":" << std::endl;
		pb.PrintDebugString();
	}
	return pb;
}

void ParseMessage(MessageHeader &msg_header, void *buffer) {
	switch (msg_header.type) {
	case PbMessageType::Version: {
		MumbleProto::Version v = ConstructProtobufObject<MumbleProto::Version>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::Ping: {
		MumbleProto::Ping cs = ConstructProtobufObject<MumbleProto::Ping>(buffer, msg_header.length, false);
		break;
	}
	case PbMessageType::CryptSetup: {
		MumbleProto::CryptSetup cs = ConstructProtobufObject<MumbleProto::CryptSetup>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::CodecVersion: {
		MumbleProto::CodecVersion cs = ConstructProtobufObject<MumbleProto::CodecVersion>(buffer, msg_header.length, true);
		break;
	}
	default:
		std::cout << ">> Unhandled message - Type: " << msg_header.type << " Length: " << msg_header.length << std::endl;
	}
}

char *Pk11GetPassword(PK11SlotInfo *slot, PRBool retry, void *arg) {
	return NULL;
}

void handshakeCallback(PRFileDesc *fd, void *client_data) {
	std::cout << "Handshake completed" << std::endl;
}

SECStatus badCertificateCallback(void *arg, PRFileDesc *fd) {
	std::cout << "Cert check failed" << std::endl;
	return SECSuccess;
}

int main(int argc, char *argv[]) {
	/* NSS Init */
	PK11_SetPasswordFunc(Pk11GetPassword);
	NSS_InitReadWrite(Settings::getSSLDbPath().c_str());
	NSS_SetDomesticPolicy();

	PK11SlotInfo *slot = PK11_GetInternalKeySlot();
	if (!slot) {
		std::cout << "PK11_GetInternalKeySlot failed";
		exit(1);
	}

	if (PK11_NeedUserInit(slot)) {
		PK11_InitPin(slot, NULL, NULL);
	}

	/* Resolve hostname */
	std::cerr << "Resolving host " << Settings::getHost() << std::endl;

	char buf[PR_NETDB_BUF_SIZE];
	PRHostEnt host_entry;
	if (PR_GetHostByName(Settings::getHost().c_str(), buf, sizeof(buf), &host_entry) == PR_FAILURE) {
		std::cout << "unknown host name: " << Settings::getHost() << std::endl;
		exit(1);
	}

	PRNetAddr addr;
	/*PRIntn er =*/ PR_EnumerateHostEnt(0, &host_entry, Settings::getPort(), &addr);

	PRFileDesc *udp_socket = PR_NewUDPSocket();
	PRFileDesc *tcp_socket = PR_NewTCPSocket();

	/* Connect */
	std::cerr << "Connecting..." << std::endl;
	if (PR_Connect(tcp_socket, &addr, PR_SecondsToInterval(30)) != PR_SUCCESS) {
		std::cerr << "Connection failed" << std::endl;
		exit(1);
	}
	PRStatus ps = PR_Connect(udp_socket, &addr, PR_INTERVAL_NO_TIMEOUT);

	/* Set SSL options */
	SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_FALSE);
	SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_FALSE);
	SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, PR_FALSE);
	SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);

	PRFileDesc *ssl_socket = SSL_ImportFD(NULL, tcp_socket);
	SSL_HandshakeCallback(ssl_socket, handshakeCallback, NULL);
	SSL_BadCertHook(ssl_socket, badCertificateCallback, NULL);
	SSL_SetURL(ssl_socket, Settings::getHost().c_str());

	/* Force SSL handshake */
	SSL_ResetHandshake(ssl_socket, PR_FALSE);
	SSL_ForceHandshake(ssl_socket);

	/* Send initial messages */
	MumbleProto::Version v;
	v.set_version(MUMBLE_VERSION(1, 2, 2));
	v.set_release("0.0.1-dev");
	sendMessage(ssl_socket, PbMessageType::Version, v, true);

	MumbleProto::Authenticate a;
	a.set_username("testBot");
	a.set_password("");
	// FIXME: hardcoded version number
	a.add_celt_versions(0x8000000b);
	sendMessage(ssl_socket, PbMessageType::Authenticate, a, true);

	/* Net event loop */
	PRPollDesc pds[2];
	pds[0].fd = ssl_socket;
	pds[0].in_flags = PR_POLL_READ; // | PR_POLL_WRITE;
	pds[1].fd = udp_socket;
	pds[1].in_flags = PR_POLL_READ; // | PR_POLL_WRITE;

	PRIntervalTime ping_interval = PR_SecondsToInterval(5);
	PRIntervalTime ping_timer = PR_IntervalNow();

	while (PR_TRUE) {
		int32 ret = PR_Poll(pds, 2, PR_SecondsToInterval(5));
		PR_ASSERT(ret != 0);
		if (ret == -1) {
			std::cerr << "PR_Poll failed" << std::endl;
			exit(1);
		}

		/* Ping handling */
		std::cout << (PR_IntervalNow() - ping_timer) << std::endl;
		if ((PRIntervalTime)(PR_IntervalNow() - ping_timer) > ping_interval) {
			ping_timer = PR_IntervalNow();
			MumbleProto::Ping p;
			p.set_timestamp(PR_Now());
			sendMessage(ssl_socket, PbMessageType::Ping, p, false);

			if (ret == 0)
				continue;
		}

		for (int i = 0; i < 2; ++i) {
			if (pds[i].out_flags & PR_POLL_READ) {
				while (PR_TRUE) {
					/* Receive message header */
					MessageHeader msg_header;
					ret = PR_Recv(pds[i].fd, &msg_header, sizeof(msg_header), NULL, PR_INTERVAL_NO_TIMEOUT);
					if (ret == -1) {
						std::cerr << "PR_Recv failed" << std::endl;
						continue;
					}

					msg_header.type = PR_ntohs(msg_header.type);
					msg_header.length = PR_ntohl(msg_header.length);

					if (msg_header.length >= 0x7FFFF)
						exit(1);

					/* Receive message body */
					char *buffer;
					buffer = (char *)PR_Malloc(msg_header.length);
					ret = PR_Recv(pds[i].fd, buffer, msg_header.length, NULL, PR_INTERVAL_NO_TIMEOUT);

					ParseMessage(msg_header, buffer);
					PR_Free(buffer);
					break;
				}
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
