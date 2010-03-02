#ifndef CLIENT_LIB_H_
#define CLIENT_LIB_H_

class MumbleClient;

class MumbleClientLib {
  public:
	static MumbleClientLib* instance();
	MumbleClient* NewClient();

  private:
	MumbleClientLib();
	~MumbleClientLib();

	static MumbleClientLib* instance_;

	MumbleClientLib(const MumbleClientLib&);
	void operator=(const MumbleClientLib&);
};

#endif  // CLIENT_LIB_H_
