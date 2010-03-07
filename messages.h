#ifndef MESSAGES_H_
#define MESSAGES_H_

namespace MumbleClient {

namespace UdpMessageType {
	enum MessageType {
		UDPVoiceCELTAlpha,
		UDPPing,
		UDPVoiceSpeex,
		UDPVoiceCELTBeta
	};
}  //end namespace UdpMessageType

namespace PbMessageType {
	enum MessageType {
		Version,
		UDPTunnel,
		Authenticate,
		Ping,
		Reject,
		ServerSync,
		ChannelRemove,
		ChannelState,
		UserRemove,
		UserState,
		BanList,
		TextMessage,
		PermissionDenied,
		ACL,
		QueryUsers,
		CryptSetup,
		ContextActionAdd,
		ContextAction,
		UserList,
		VoiceTarget,
		PermissionQuery,
		CodecVersion,
		UserStats,
		RequestBlob,
		ServerConfig
	};
}  // end namespace PbMessageType

}  // end namespace MumbleClient

#endif  // MESSAGES_H_
