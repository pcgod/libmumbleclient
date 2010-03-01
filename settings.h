#ifndef SETTINGS_H
#define SETTINGS_H

class Settings {
  public:

	static std::string getSSLDbPath() {
		return "sql:.";
	}

	static std::string getHost() {
		return "0xy.de";
	}

	static int32 getPort() {
		return 64379;
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

#endif
