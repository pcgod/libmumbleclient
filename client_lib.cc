#include "client_lib.h"

#include <iostream>

#include "client.h"
#include "settings.h"

///////////////////////////////////////////////////////////////////////////////

// static
MumbleClientLib *MumbleClientLib::instance_ = NULL;

// static
char* MumbleClientLib::Pk11GetPassword(PK11SlotInfo* /* slot */, PRBool /* retry */, void* /* arg */) {
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// MumbleClientLib, private:

MumbleClientLib::MumbleClientLib() {
	// NSS Init
	PK11_SetPasswordFunc(Pk11GetPassword);
	NSS_InitReadWrite(Settings::getSSLDbPath().c_str());
	NSS_SetDomesticPolicy();

	slot_ = PK11_GetInternalKeySlot();
	if (slot_ == NULL) {
		std::cout << "PK11_GetInternalKeySlot failed";
		exit(1);
	}

	if (PK11_NeedUserInit(slot_)) {
		PK11_InitPin(slot_, NULL, NULL);
	}
}

MumbleClientLib::~MumbleClientLib() {
	if (slot_)
		PK11_FreeSlot(slot_);

	NSS_Shutdown();
}

///////////////////////////////////////////////////////////////////////////////
// MumbleClientLib, public:

MumbleClientLib *MumbleClientLib::instance() {
	if (instance_ == NULL) {
		instance_ = new MumbleClientLib();
		return instance_;
	}

	return instance_;
}

MumbleClient *MumbleClientLib::NewClient() {
	return new MumbleClient();
}
