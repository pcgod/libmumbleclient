#include <iostream>

#include "client.h"
#include "client_lib.h"

int main(int /* argc */, char** /* argv[] */) {
	MumbleClientLib* mcl = MumbleClientLib::instance();

	// Create a new client
	MumbleClient* mc = mcl->NewClient();
	mc->Connect();

	// Start event loop
	mcl->Run();
}
