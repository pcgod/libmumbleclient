#ifndef CLIENT_LIB_H_
#define CLIENT_LIB_H_

#include "nss.h"
#include "pk11pub.h"
#include "ssl.h"

class MumbleClient;

class MumbleClientLib {
  public:
	static MumbleClientLib* instance();
	MumbleClient* NewClient();

  private:
	MumbleClientLib();
	~MumbleClientLib();

	static char* Pk11GetPassword(PK11SlotInfo*, PRBool, void*);

	static MumbleClientLib* instance_;
	PK11SlotInfo* slot_;

	MumbleClientLib(const MumbleClientLib&);
	void operator=(const MumbleClientLib&);
};

#endif  // CLIENT_LIB_H_
