#include "client.h"

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <deque>
#include <fstream>
#include <iostream>
#include <typeinfo>

#include "settings.h"

using mumble_message::MessageHeader;
using mumble_message::Message;

///////////////////////////////////////////////////////////////////////////////

#define MUMBLE_VERSION(x, y, z) ((x << 16) | (y << 8) | (z & 0xFF))

namespace {

template <class T> T ConstructProtobufObject(void* buffer, int32_t length, bool print) {
	T pb;
	pb.ParseFromArray(buffer, length);
	if (print) {
		std::cout << ">> IN: " << typeid(T).name() << ":" << std::endl;
		pb.PrintDebugString();
	}
	return pb;
}

}  // namespace


///////////////////////////////////////////////////////////////////////////////
// MumbleClient, private:

MumbleClient::MumbleClient(boost::asio::io_service* io_service) : io_service_(io_service), state_(kStateNew), ping_timer_(NULL) {
}

void MumbleClient::DoPing(const boost::system::error_code& error) {
	if (error) {
		std::cerr << "ping error: " << error.message() << std::endl;
		return;
	}

	MumbleProto::Ping p;
	p.set_timestamp(std::time(NULL));
	sendMessage(PbMessageType::Ping, p, false);

	// Requeue ping
	if (!ping_timer_)
		ping_timer_ = new boost::asio::deadline_timer(*io_service_);

	ping_timer_->expires_from_now(boost::posix_time::seconds(5));
	ping_timer_->async_wait(boost::bind(&MumbleClient::DoPing, this, boost::asio::placeholders::error));
}

void MumbleClient::ParseMessage(const MessageHeader& msg_header, void* buffer) {
	switch (msg_header.type) {
	case PbMessageType::Version: {
		MumbleProto::Version v = ConstructProtobufObject<MumbleProto::Version>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::Ping: {
		MumbleProto::Ping p = ConstructProtobufObject<MumbleProto::Ping>(buffer, msg_header.length, false);
		break;
	}
	case PbMessageType::CryptSetup: {
		MumbleProto::CryptSetup cs = ConstructProtobufObject<MumbleProto::CryptSetup>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::CodecVersion: {
		MumbleProto::CodecVersion cv = ConstructProtobufObject<MumbleProto::CodecVersion>(buffer, msg_header.length, true);
		break;
	}
	case PbMessageType::ServerSync: {
		MumbleProto::ServerSync ss = ConstructProtobufObject<MumbleProto::ServerSync>(buffer, msg_header.length, true);
		state_ = kStateAuthenticated;
		// Enqueue ping
		DoPing(boost::system::error_code());
		break;
	}
	case PbMessageType::UDPTunnel: {
		std::fstream fs("udptunnel.out", std::fstream::app | std::fstream::out | std::fstream::binary);
		fs.write(reinterpret_cast<const char *>(&msg_header.length), 4);
		fs.write(reinterpret_cast<char *>(buffer), msg_header.length);
		fs.close();
		break;
	}
	default:
		std::cout << ">> IN: Unhandled message - Type: " << msg_header.type << " Length: " << msg_header.length << std::endl;
	}
}

void MumbleClient::ProcessTCPSendQueue(const boost::system::error_code& error, const size_t /*bytes_transferred*/) {
	if (!error) {
		send_queue_.pop_front();

		if (send_queue_.empty())
			return;

		Message& msg = send_queue_.front();

		std::vector<boost::asio::const_buffer> bufs;
		bufs.push_back(boost::asio::buffer(reinterpret_cast<char *>(&msg.header_), sizeof(msg.header_)));
		bufs.push_back(boost::asio::buffer(msg.msg_, msg.msg_.size()));

		async_write(*tcp_socket_, bufs, boost::bind(&MumbleClient::ProcessTCPSendQueue, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		std::cout << "<< ASYNC Type: " << ntohs(msg.header_.type) << " Length: 6+" << msg.msg_.size() << std::endl;
	} else {
		std::cerr << "Write error: " << error.message() << std::endl;
	}
}

///////////////////////////////////////////////////////////////////////////////
// MumbleClient, public:

MumbleClient::~MumbleClient() {
	delete ping_timer_;
	delete tcp_socket_;
	delete udp_socket_;
}

void MumbleClient::Connect() {
	// Resolve hostname
	std::cerr << "Resolving host " << Settings::getHost() << std::endl;

	tcp::resolver resolver(*io_service_);
	tcp::resolver::query query(Settings::getHost(), Settings::getPort());
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;

	// Try to connect
#if SSL
	boost::asio::ssl::context ctx(*io_service_, boost::asio::ssl::context::tlsv1);
	tcp_socket_ = new boost::asio::ssl::stream<tcp::socket>(*io_service_, ctx);
#else
	tcp_socket_ = new tcp::socket(*io_service_);
#endif
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end) {
		std::cerr << "Connecting to " << (*endpoint_iterator).endpoint().address() << " ..." << std::endl;
#if SSL
		tcp_socket_->lowest_layer().close();
		tcp_socket_->lowest_layer().connect(*endpoint_iterator++, error);
#else
		tcp_socket_->close();
		tcp_socket_->connect(*endpoint_iterator++, error);
#endif
	}
	if (error) {
		std::cerr << "connection error: " << error.message() << std::endl;
		exit(1);
	}

#if SSL
	udp::endpoint udp_endpoint((*endpoint_iterator).endpoint().address(), (*endpoint_iterator).endpoint().port());
	udp_socket_ = new udp::socket(*io_service_);
	udp_socket_->connect(udp_endpoint, error);

	// Do SSL handshake
	tcp_socket_->handshake(boost::asio::ssl::stream_base::client, error);
	if (error) {
		std::cerr << "handshake error: " << error.message() << std::endl;
		exit(1);
	}
#endif

	std::cout << "Handshake completed" << std::endl;
	state_ = kStateHandshakeCompleted;

	boost::asio::socket_base::non_blocking_io nbio_command(true);
#if SSL
	tcp_socket_->lowest_layer().io_control(nbio_command);
#else
	tcp_socket_->io_control(nbio_command);
#endif

	// Send initial messages
	MumbleProto::Version v;
	v.set_version(MUMBLE_VERSION(1, 2, 2));
	v.set_release("0.0.1-dev");
	sendMessage(PbMessageType::Version, v, true);

	MumbleProto::Authenticate a;
	a.set_username(Settings::getUserName());
	a.set_password(Settings::getPassword());
	// FIXME: hardcoded version number
	a.add_celt_versions(0x8000000b);
	sendMessage(PbMessageType::Authenticate, a, true);

	tcp_socket_->async_read_some(boost::asio::null_buffers(), boost::bind(&MumbleClient::ReadWriteHandler, this, boost::asio::placeholders::error));
}

void MumbleClient::ReadWriteHandler(const boost::system::error_code& error) {
	if (error) {
		std::cerr << "read error: " << error.message() << std::endl;
		return;
	}

	// TCP socket handling - read
	while (true) {
		// Receive message header
		MessageHeader msg_header;
		read(*tcp_socket_, boost::asio::buffer(reinterpret_cast<char *>(&msg_header), 6));

		msg_header.type = ntohs(msg_header.type);
		msg_header.length = ntohl(msg_header.length);

		if (msg_header.length >= 0x7FFFF)
			exit(1);

		// Receive message body
		char* buffer = static_cast<char *>(malloc(msg_header.length));
		read(*tcp_socket_, boost::asio::buffer(buffer, msg_header.length));

		ParseMessage(msg_header, buffer);
		free(buffer);
		break;
	}

	// Requeue read
	tcp_socket_->async_read_some(boost::asio::null_buffers(), boost::bind(&MumbleClient::ReadWriteHandler, this, boost::asio::placeholders::error));
}

void MumbleClient::sendMessage(PbMessageType::MessageType type, const ::google::protobuf::Message& new_msg, bool print) {
	if (print) {
		std::cout << "<< ENQUEUE: " << type << std::endl;
		new_msg.PrintDebugString();
	}

	bool write_in_progress = !send_queue_.empty();
	int32_t length = new_msg.ByteSize();
	MessageHeader msg_header;
	msg_header.type = htons(static_cast<int16_t>(type));
	msg_header.length = htonl(length);

	std::string pb_message = new_msg.SerializeAsString();

	Message message(msg_header, pb_message);
	send_queue_.push_back(message);

	if (state_ >= kStateHandshakeCompleted && !write_in_progress) {
		Message& msg = send_queue_.front();

		std::vector<boost::asio::const_buffer> bufs;
		bufs.push_back(boost::asio::buffer(reinterpret_cast<char *>(&msg.header_), sizeof(msg.header_)));
		bufs.push_back(boost::asio::buffer(msg.msg_, msg.msg_.size()));
		async_write(*tcp_socket_, bufs, boost::bind(&MumbleClient::ProcessTCPSendQueue, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		std::cout << "<< ASYNC Type: " << ntohs(msg.header_.type) << " Length: 6+" << msg.msg_.size() << std::endl;
	}
}
