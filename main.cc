#include "celt.h"
#ifdef WITH_MPG123
#include <mpg123.h>
#endif

#include <iostream>
#include <fstream>

#include <boost/make_shared.hpp>
#include <boost/thread.hpp>

#include "client.h"
#include "client_lib.h"
#include "CryptState.h"
#include "messages.h"
#include "PacketDataStream.h"
#include "settings.h"

namespace {

// Always 48000 for Mumble
const int32_t kSampleRate = 48000;

bool recording = false;
bool playback = false;

boost::thread *playback_thread = NULL;

inline int32_t pds_int_len(char* x) {
	if ((x[0] & 0x80) == 0x00) {
		return 1;
	} else if ((x[0] & 0xC0) == 0x80) {
		return 2;
	} else if ((x[0] & 0xF0) == 0xF0) {
		switch (x[0] & 0xFC) {
			case 0xF0:
				return 5;
			case 0xF4:
				return 9;
			case 0xF8:
				return pds_int_len(&x[1]) + 1;
			case 0xFC:
				return 1;
			default:
				return 1;
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

#ifdef WITH_MPG123
void playMp3(MumbleClient::MumbleClient* mc) {
	std::cout << "<< play mp3 thread" << std::endl;

//	struct sched_param param;
//	param.sched_priority = 1;
//	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

	// FIXME(pcgod): 1-6 to match Mumble client
	int frames = 6;
//	int audio_quality = 96000;
	int audio_quality = 60000;

	int err = mpg123_init();
	mpg123_handle *mh = mpg123_new(NULL, &err);
	mpg123_param(mh, MPG123_VERBOSE, 255, 0);
	mpg123_param(mh, MPG123_RVA, MPG123_RVA_MIX, 0);
	mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_MONO_MIX, 0);
	mpg123_param(mh, MPG123_FORCE_RATE, kSampleRate, 0);
	mpg123_open(mh, "<file>");

	long rate = 0;
	int channels = 0, encoding = 0;
	mpg123_getformat(mh, &rate, &channels, &encoding);
	mpg123_format_none(mh);

	rate = kSampleRate;
	channels = MPG123_MONO;
	err = mpg123_format(mh, rate, channels, encoding);

	// FIXME(pcgod): maybe broken for mono MP3s
	size_t buffer_size = (kSampleRate / 100) * 2/* * channels*/;

	CELTMode *cmMode = celt_mode_create(kSampleRate, kSampleRate / 100, NULL);
	CELTEncoder *ce = celt_encoder_create(cmMode, 1, NULL);

	celt_encoder_ctl(ce, CELT_SET_PREDICTION(0));
	celt_encoder_ctl(ce, CELT_SET_VBR_RATE(audio_quality));

	std::deque<std::string> packet_list;

	std::cout << "decoding..." << std::endl;
	unsigned char* buffer = static_cast<unsigned char *>(malloc(buffer_size));
	do {
		size_t done = 0;
		unsigned char out[512];

		err = mpg123_read(mh, buffer, buffer_size, &done);
		int32_t len = celt_encode(ce, reinterpret_cast<short *>(buffer), NULL, out, std::min(audio_quality / (100 * 8), 127));
		// std::cout << (done / sizeof(short)) << " samples - bitrate: " << (len * 100 * 8) << std::endl;

		packet_list.push_back(std::string(reinterpret_cast<char *>(out), len));
	} while (err == MPG123_OK);

	if (err != MPG123_DONE)
		std::cerr << "Warning: Decoding ended prematurely because: " << (err == MPG123_ERR ? mpg123_strerror(mh) : mpg123_plain_strerror(err)) << std::endl;

	std::cout << "finished decoding" << std::endl;

	free(buffer);
	celt_encoder_destroy(ce);
	celt_mode_destroy(cmMode);
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();


	int32_t seq = 0;
	while (playback && !packet_list.empty()) {
		// build pds
		char data[1024];
		int flags = 0; // target = 0
		flags |= (MumbleClient::UdpMessageType::UDPVoiceCELTAlpha << 5);
		data[0] = static_cast<unsigned char>(flags);

		MumbleClient::PacketDataStream pds(data + 1, 1023);
		seq += frames;
		pds << seq;
		// Append |frames| frames to pds
		for (int i = 0; i < frames; ++i) {
			if (packet_list.empty()) break;
			const std::string& s = packet_list.front();

			unsigned char head = s.size();
			// Add 0x80 to all but the last frame
			if (i < frames - 1)
				head |= 0x80;

			pds.append(head);
			pds.append(s);

			packet_list.pop_front();
		}

#define TCP 0
#if TCP
		mc->SendRawUdpTunnel(data, pds.size() + 1);
#else
		mc->SendUdpMessage(data, pds.size() + 1);
#endif
		boost::this_thread::sleep(boost::posix_time::milliseconds((frames) * 10));
	}

	playback = false;
	std::cout << ">> play mp3 thread" << std::endl;
}
#endif

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
#ifdef WITH_MPG123
	} else if (message == "playmp3") {
		if (playback == false) {
			playback = true;
			playback_thread = new boost::thread(playMp3, mc);
		}
#endif
	} else if (message == "stop") {
		recording = false;
		playback = false;
#ifndef NDEBUG
	} else if (message == "channellist") {
		mc->PrintChannelList();
	} else if (message == "userlist") {
		mc->PrintUserList();
#endif
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

struct RelayMessage {
	MumbleClient::MumbleClient* mc;
	const std::string message;
	RelayMessage(MumbleClient::MumbleClient* mc_, const std::string& message_) : mc(mc_), message(message_) { }
};

boost::condition_variable cond;
boost::mutex mut;
std::deque< boost::shared_ptr<RelayMessage> > relay_queue;

void RelayThread() {
	boost::unique_lock<boost::mutex> lock(mut);
	while (true) {
		while (relay_queue.empty()) {
			cond.wait(lock);
		}

		boost::shared_ptr<RelayMessage>& r = relay_queue.front();
		r->mc->SendRawUdpTunnel(r->message.data(), r->message.size());
		relay_queue.pop_front();
	}
}

void RelayTunnelCallback(int32_t length, void* buffer, MumbleClient::MumbleClient* mc) {
	std::string s(static_cast<char *>(buffer), length);
	s.erase(1, pds_int_len(&static_cast<char *>(buffer)[1]));
	boost::shared_ptr<RelayMessage> r = boost::make_shared<RelayMessage>(mc, s);
	{
		boost::lock_guard<boost::mutex> lock(mut);
		relay_queue.push_back(r);
	}
	cond.notify_all();
}

}  // namespace

int main(int /* argc */, char** /* argv[] */) {
	MumbleClient::MumbleClientLib* mcl = MumbleClient::MumbleClientLib::instance();

	MumbleClient::MumbleClientLib::SetLogLevel(0);
	//MumbleClient::MumbleClientLib::SetLogLevel(1);

	// Create a new client
	MumbleClient::MumbleClient* mc = mcl->NewClient();
	//MumbleClient::MumbleClient* mc2 = mcl->NewClient();

	mc->Connect(MumbleClient::Settings("0xy.org", "64739", "testBot", ""));
	//mc2->Connect(MumbleClient::Settings("0xy.org", "64738", "testBot2", ""));

	mc->SetAuthCallback(boost::bind(&AuthCallback));
	mc->SetTextMessageCallback(boost::bind(&TextMessageCallback, _1, mc));
	mc->SetRawUdpTunnelCallback(boost::bind(&RawUdpTunnelCallback, _1, _2));

	//mc->SetRawUdpTunnelCallback(boost::bind(&RelayTunnelCallback, _1, _2, mc2));
	//mc2->SetRawUdpTunnelCallback(boost::bind(&RelayTunnelCallback, _1, _2, mc));

	//boost::thread relay_thread = boost::thread(RelayThread);

//	MumbleClient::MumbleClient* mc2 = mcl->NewClient();
//	mc2->Connect(MumbleClient::Settings("0xy.org", "64739", "testBot2", ""));
//	mc2->SetTextMessageCallback(boost::bind(&TextMessageCallback, _1, mc2));
//	mc2->SetRawUdpTunnelCallback(boost::bind(&RawUdpTunnelCallback, _1, _2));

	// Start event loop
	mcl->Run();
	delete mc;

	mcl->Shutdown();
	mcl = NULL;
}
