#ifndef SETTINGS_H_
#define SETTINGS_H_

class Settings {
  public:

	static std::string getSSLDbPath() {
		return "sql:.";
	}

	static std::string getHost() {
		return "0xy.de";
	}

	static std::string getPort() {
		return "64739";
	}

	static std::string getUserName() {
		return "testBot";
	}

	static std::string getPassword() {
		return "";
	}

  private:
	Settings();
	Settings(const Settings&);
	void operator=(const Settings&);
};

#endif  // SETTINGS_H_
