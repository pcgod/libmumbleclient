#include "client_lib.h"

#include <iostream>

#include "client.h"
#include "logging.h"
#include "settings.h"

namespace MumbleClient {

///////////////////////////////////////////////////////////////////////////////

// static
MumbleClientLib* MumbleClientLib::instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// MumbleClientLib, private:

MumbleClientLib::MumbleClientLib() {
}

MumbleClientLib::~MumbleClientLib() {
	delete instance_;
}

///////////////////////////////////////////////////////////////////////////////
// MumbleClientLib, public:

MumbleClientLib* MumbleClientLib::instance() {
	if (instance_ == NULL) {
		instance_ = new MumbleClientLib();
		return instance_;
	}

	return instance_;
}

MumbleClient* MumbleClientLib::NewClient() {
	return new MumbleClient(&io_service_);
}

void MumbleClientLib::Run() {
	io_service_.reset();
	io_service_.run();
}

void MumbleClientLib::Shutdown() {
	::google::protobuf::ShutdownProtobufLibrary();
}

// static
int32_t MumbleClientLib::GetLogLevel() {
	return logging::GetLogLevel();
}

// static
void MumbleClientLib::SetLogLevel(int32_t level) {
	logging::SetLogLevel(level);
}

}  // namespace MumbleClient
