#ifndef CHANNEL_H_
#define CHANNEL_H_

namespace MumbleClient {

class Channel {
  public:
	Channel(int32_t id_) : id(id_) { std::cout << "New channel" << std::endl; }
	~Channel() { std::cout << "Channel " << name << " destroyed" << std::endl; }
	int32_t id;
	boost::weak_ptr<Channel> parent;
	int32_t position;
	bool temporary;
	std::string name;
	std::string description;
//	std::vector<Channel> links;

  private:
	Channel(const Channel&);
	void operator=(const Channel&);
};

}  // end namespace MumbleClient

#endif  // CHANNEL_H_
