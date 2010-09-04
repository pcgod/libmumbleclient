#ifndef USER_H_
#define USER_H_

#include <boost/weak_ptr.hpp>

#include "logging.h"

namespace MumbleClient {

class Channel;

class User {
  public:
	User(int32_t session_, boost::shared_ptr<Channel> channel_) : session(session_), channel(channel_) { }
	~User() { DLOG(INFO) << "User " << name << " destroyed"; }
	int32_t session;
	int32_t user_id;
	boost::weak_ptr<Channel> channel;
	bool mute;
	bool deaf;
	bool suppress;
	bool self_mute;
	bool self_deaf;
	std::string name;
	std::string comment;
	std::string hash;

  private:
	User(const User&);
	void operator=(const User&);

};

}  // namespace MumbleClient

#endif  // USER_H_
