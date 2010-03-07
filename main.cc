#include <iostream>
#include <fstream>

#include "client.h"
#include "client_lib.h"
#include "settings.h"

bool recording = false;

void AuthCallback() {
	std::cout << "I'm authenticated" << std::endl;
}

void TextMessageCallback(const std::string& message, MumbleClient::MumbleClient *mc) {
	if (message == "record") {
		recording = true;
	} else if (message == "stop") {
		recording = false;
	}

	std::cout << "TM: " << message << std::endl;
	mc->SetComment(message);
}

void RawUdpTunnelCallback(int32_t length, void* buffer) {
	if (!recording)
		return;

	std::fstream fs("udptunnel.out", std::fstream::app | std::fstream::out | std::fstream::binary);
	fs.write(reinterpret_cast<const char *>(length), 4);
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
}
