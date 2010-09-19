#ifndef SETTINGS_H_
#define SETTINGS_H_

namespace MumbleClient {

class DLL_PUBLIC Settings {
  public:
	Settings(const std::string& host, const std::string& port,
			 const std::string& user_name, const std::string& password) :
				host_(host),
				port_(port),
				user_name_(user_name),
				password_(password) { }

	std::string GetHost() const { return host_; }
	std::string GetPort() const { return port_; }
	std::string GetUserName() const { return user_name_; }
	std::string GetPassword() const { return password_; }

	void SetHost(const std::string& host) { host_ = host; }
	void SetPort(const std::string& port) { port_ = port; }
	void SetUserName(const std::string& user_name) { user_name_ = user_name; }
	void SetPassword(const std::string& password) { password_ = password; }

  private:
	std::string host_;
	std::string port_;
	std::string user_name_;
	std::string password_;
	DLL_LOCAL Settings(const Settings&);
	DLL_LOCAL void operator=(const Settings&);
};

}  // namespace MumbleClient

#endif  // SETTINGS_H_
