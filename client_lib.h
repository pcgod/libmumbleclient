#ifndef CLIENT_LIB_H_
#define CLIENT_LIB_H_

#include <boost/asio.hpp>

class MumbleClient;

class MumbleClientLib {
  public:
	static MumbleClientLib* instance();
	MumbleClient* NewClient();
	void Run();

  private:
	MumbleClientLib();
	~MumbleClientLib();

	static MumbleClientLib* instance_;
	boost::asio::io_service io_service_;

	MumbleClientLib(const MumbleClientLib&);
	void operator=(const MumbleClientLib&);
};

#endif  // CLIENT_LIB_H_
