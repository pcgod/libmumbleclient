#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <boost/weak_ptr.hpp>

#include "logging.h"

namespace MumbleClient {

class Channel {
  public:
	Channel(int32_t id_) : id(id_) { }
	~Channel() { DLOG(INFO) << "Channel " << name << " destroyed"; }
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

}  // namespace MumbleClient

#endif  // CHANNEL_H_
