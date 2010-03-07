#include <iostream>
#include <fstream>
#include <boost/thread.hpp>

#include "client.h"
#include "client_lib.h"
#include "CryptState.h"
#include "messages.h"
#include "settings.h"

bool recording = false;
bool playback = false;

boost::thread *playback_thread = NULL;

static inline int32_t pds_int_len(char* x) {
	if ((x[0] & 0x80) == 0x00) {
		return 1;
	} else if ((x[0] & 0xC0) == 0x80) {
		return 2;
	} else if ((x[0] & 0xF0) == 0xF0) {
		switch (x[0] & 0xFC) {
			case 0xF0:
				return 5;
				break;
			case 0xF4:
				return 9;
				break;
			case 0xF8:
				return pds_int_len(&x[1]) + 1;
				break;
			case 0xFC:
				return 1;
				break;
			default:
				return 1;
				break;
		}
	} else if ((x[0] & 0xF0) == 0xE0) {
		return 3;
	} else if ((x[0] & 0xE0) == 0xC0) {
		return 3;
	}

	return 0;
}

int scanPacket(char* data, int len) {
	int header = 0;
	int frames = 0;
	// skip flags
	int pos = 1;

	// skip session & seqnr
	pos += pds_int_len(&data[pos]);
	pos += pds_int_len(&data[pos]);

	bool valid = true;
	do {
		header = static_cast<unsigned char>(data[pos]);
		++pos;
		++frames;
		pos += (header & 0x7f);

		if (pos > len)
			valid = false;
	} while ((header & 0x80) && valid);

	if (valid) {
		return frames;
	} else {
		return -1;
	}
}

void playbackFunction(MumbleClient::MumbleClient* mc) {
	std::cout << "<< playback thread" << std::endl;

	std::ifstream fs("udptunnel.out", std::ios::binary);
	while (playback && !fs.eof()) {
		int l = 0;
		fs.read(reinterpret_cast<char *>(&l), 4);

		if (!l)
			break;

		char *buffer = static_cast<char *>(malloc(l));
//		std::cout << "len: " << l << " pos: " << fs.tellg() << std::endl;
		fs.read(buffer, l);

		int frames = scanPacket(buffer, l);
//		std::cout << "scan: " << frames << " " << ceil(1000.0 / 10 / frames) << std::endl;

		buffer[0] = MumbleClient::UdpMessageType::UDPVoiceCELTAlpha | 0;
		memcpy(&buffer[1], &buffer[2], l - 1);

#define TCP 0
#if TCP
		mc->SendRawUdpTunnel(buffer, l - 1);
#else
		mc->SendUdpMessage(buffer, l - 1);
#endif

		boost::this_thread::sleep(boost::posix_time::milliseconds(frames * 10));
		free(buffer);
	}

	fs.close();
	playback = false;
	std::cout << ">> playback thread" << std::endl;
}

void AuthCallback() {
	std::cout << "I'm authenticated" << std::endl;
}

void TextMessageCallback(const std::string& message, MumbleClient::MumbleClient* mc) {
	if (message == "record") {
		recording = true;
	} else if (message == "play") {
		if (playback == false) {
			playback = true;
			playback_thread = new boost::thread(playbackFunction, mc);
		}
	} else if (message == "stop") {
		recording = false;
		playback = false;
	} else if (message == "quit") {
		mc->Disconnect();
	}

	std::cout << "TM: " << message << std::endl;

	if (message != "quit")
		mc->SetComment(message);
}

void RawUdpTunnelCallback(int32_t length, void* buffer) {
	if (!recording)
		return;

	std::fstream fs("udptunnel.out", std::fstream::app | std::fstream::out | std::fstream::binary);
	fs.write(reinterpret_cast<const char *>(&length), 4);
	fs.write(reinterpret_cast<char *>(buffer), length);
	fs.close();
}

int main(int /* argc */, char** /* argv[] */) {
	MumbleClient::MumbleClientLib* mcl = MumbleClient::MumbleClientLib::instance();

	// Create a new client
	MumbleClient::MumbleClient* mc = mcl->NewClient();
	mc->Connect(MumbleClient::Settings("0xy.org", "64739", "testBot", ""));

	mc->SetAuthCallback(boost::bind(&AuthCallback));
	mc->SetTextMessageCallback(boost::bind(&TextMessageCallback, _1, mc));
	mc->SetRawUdpTunnelCallback(boost::bind(&RawUdpTunnelCallback, _1, _2));

//	MumbleClient::MumbleClient* mc2 = mcl->NewClient();
//	mc2->Connect(MumbleClient::Settings("0xy.org", "64739", "testBot2", ""));

	// Start event loop
	mcl->Run();
	delete mc;

	mcl->Shutdown();
	mcl = NULL;
}
