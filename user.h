#ifndef USER_H_
#define USER_H_

namespace MumbleClient {

class Channel;

class User {
  public:
	User(int32_t session_, boost::shared_ptr<Channel> channel_) : session(session_), channel(channel_) { std::cout << "New user" << std::endl; }
	~User() { std::cout << "User " << name << " destroyed" << std::endl; }
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

}  // end namespace MumbleClient

#endif  // USER_H_
