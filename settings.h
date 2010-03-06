#ifndef SETTINGS_H_
#define SETTINGS_H_

namespace MumbleClient {

class Settings {
  public:
	std::string getHost() const {
		return "0xy.de";
	}

	std::string getPort() const {
		return "64739";
	}

	std::string getUserName() const {
		return "testBot";
	}

	std::string getPassword() const {
		return "";
	}
/*
  private:
	Settings();
	Settings(const Settings&);
	void operator=(const Settings&);
*/
};

}  // end namespace MumbleClient

#endif  // SETTINGS_H_
