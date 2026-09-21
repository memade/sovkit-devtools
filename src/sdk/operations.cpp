#include "../sdk.hpp"
#include <cstring>
#include <stdexcept>

namespace devtools {

	Json Sdk::execute(const std::string& operation, const Json& request) {
		// stop 允许在未加载时调用，便于统一处理窗口关闭。
		if (operation == "stop") {
			return stop();
		}
		if (!loaded()) {
			throw std::runtime_error("Load SDK first");
		}
		if (!request.is_object()) {
			throw std::runtime_error("Request must be a JSON object");
		}
		const std::string input = request.dump();
		if (input.size() > 1024 * 1024) {
			throw std::runtime_error("Request exceeds 1 MiB");
		}
		if (operation == "events") {
			return events();
		}
		if (operation == "selftest") {
			return SelfTest();
		}

		// 固定路由只选择具名方法；不把真正的 SDK 调用藏在宏或生成文件中。
		if (operation == "log_configure_v1") {
			return LogConfigureV1(input);
		}
		if (operation == "log_read_v1") {
			return LogReadV1(input);
		}
		if (operation == "log_emit_v1") {
			return LogEmitV1(input);
		}
		if (operation == "log_stats_v1") {
			return LogStatsV1();
		}
		if (operation == "store_query_v1") {
			return StoreQueryV1(input);
		}
		if (operation == "store_conversation_v1") {
			return StoreConversationV1(input);
		}
		if (operation == "store_result_v1") {
			return StoreResultV1(input);
		}
		if (operation == "info") {
			return Info();
		}
		if (operation == "transport_probe") {
			return TransportProbe(input);
		}
		if (operation == "identity_create") {
			return IdentityCreate(input);
		}
		if (operation == "identity_open") {
			return IdentityOpen();
		}
		if (operation == "identity_rotate_device") {
			return IdentityRotateDevice(input);
		}
		if (operation == "identity_rename_device") {
			return IdentityRenameDevice(input);
		}
		if (operation == "identity_revoke_active_device") {
			return IdentityRevokeActiveDevice(input);
		}
		if (operation == "identity_list_devices") {
			return IdentityListDevices();
		}
		if (operation == "identity_sign") {
			return IdentitySign(input);
		}
		if (operation == "identity_verify") {
			return IdentityVerify(input);
		}
		if (operation == "identity_export") {
			return IdentityExport(input);
		}
		if (operation == "identity_import") {
			return IdentityImport(input);
		}
		if (operation == "discovery_start") {
			return DiscoveryStart(input);
		}
		if (operation == "discovery_stop") {
			return DiscoveryStop();
		}
		if (operation == "discovery_status") {
			return DiscoveryStatus();
		}
		if (operation == "discovery_list") {
			return DiscoveryList();
		}
		if (operation == "proximity_invitation_create") {
			return ProximityInvitationCreate(input);
		}
		if (operation == "proximity_invitation_inspect") {
			return ProximityInvitationInspect(input);
		}
		if (operation == "pairing_start_from_invitation") {
			return PairingStartFromInvitation(input);
		}
		if (operation == "ble_transport") {
			return BleTransport(input);
		}
		if (operation == "pairing_start") {
			return PairingStart(input);
		}
		if (operation == "pairing_status") {
			return PairingStatus();
		}
		if (operation == "session_list") {
			return SessionList();
		}
		if (operation == "session_resume") {
			return SessionResume(input);
		}
		if (operation == "wan_offer_create") {
			return WanOfferCreate(input);
		}
		if (operation == "wan_offer_accept") {
			return WanOfferAccept(input);
		}
		if (operation == "wan_answer_accept") {
			return WanAnswerAccept(input);
		}
		if (operation == "network_path_list") {
			return NetworkPathList();
		}
		if (operation == "network_paths_update") {
			return NetworkPathsUpdate(input);
		}
		if (operation == "wan_route_list") {
			return WanRouteList();
		}
		if (operation == "wan_route_stop") {
			return WanRouteStop(input);
		}
		if (operation == "pairing_confirm") {
			return PairingConfirm(input);
		}
		if (operation == "pairing_cancel") {
			return PairingCancel();
		}
		if (operation == "relationship_list") {
			return RelationshipList();
		}
		if (operation == "relationship_remove") {
			return RelationshipRemove(input);
		}
		if (operation == "message_send") {
			return MessageSend(input);
		}
		if (operation == "message_list") {
			return MessageList(input);
		}
		if (operation == "message_flush") {
			return MessageFlush();
		}
		if (operation == "conversation_export") {
			return ConversationExport(input);
		}
		if (operation == "transfer_offer") {
			return TransferOffer(input);
		}
		if (operation == "transfer_decide") {
			return TransferDecide(input);
		}
		if (operation == "transfer_cancel") {
			return TransferCancel(input);
		}
		if (operation == "transfer_pause") {
			return TransferPause(input);
		}
		if (operation == "transfer_resume") {
			return TransferResume(input);
		}
		if (operation == "transfer_forget") {
			return TransferForget(input);
		}
		if (operation == "transfer_list") {
			return TransferList(input);
		}
		if (operation == "transfer_flush") {
			return TransferFlush();
		}
		if (operation == "message_forget") {
			return MessageForget(input);
		}
		if (operation == "network_share") {
			return NetworkShare(input);
		}
		if (operation == "connection_assistance") {
			return ConnectionAssistance(input);
		}
		if (operation == "get_last_error") {
			return GetLastError();
		}
		throw std::runtime_error("Unsupported operation; lifecycle, callbacks and raw key pointers are managed by the host");
	}

	Json Sdk::SelfTest() {
		std::array<uint8_t, 32> test_key{};
		const uint8_t plain[] = {0, 42, 0, 255};
		SdkBuffer encrypted(library_.Free);
		SdkBuffer decoded(library_.Free, true);
		// 二进制缓冲区含零字节，必须使用长度而不是 strlen。
		int code = library_.StorageProtect(test_key.data(), plain, sizeof(plain), &encrypted.data, &encrypted.size);
		if (code == 0) {
			code = library_.StorageUnprotect(test_key.data(), reinterpret_cast<const uint8_t*>(encrypted.data), encrypted.size, &decoded.data, &decoded.size);
		}
		const bool same = code == 0 && decoded.size == sizeof(plain) && decoded.data && std::memcmp(decoded.data, plain, sizeof(plain)) == 0;
		return {{"code", same ? 0 : -1}, {"abiVersion", library_.AbiVersion()}, {"symbols", symbol_count}, {"data", {{"binaryRoundTrip", same}}}};
	}

	Json Sdk::events() {
		Json rows = Json::array();
		if (!loaded()) {
			return rows;
		}
		// 每轮最多读取 64 条事件，避免持续事件流阻塞用户请求。
		for (int index = 0; index < 64; ++index) {
			SdkBuffer output(library_.Free);
			const int code = handle_->EventPoll(&output.data, &output.size);
			if (code == -10004) {
				break;
			} // SDK 表示当前没有事件。
			rows.push_back(MakeResponse(code, output));
			if (code != 0) {
				break;
			}
		}
		return rows;
	}

}
