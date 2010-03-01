#ifndef MESSAGES_H_
#define MESSAGES_H_

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
}

#endif  // MESSAGES_H_
