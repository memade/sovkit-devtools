#include "../sdk.hpp"

namespace devtools {

	Json Sdk::TransportProbe(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 探测指定传输配置。
		const int code = handle_->TransportProbe(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::DiscoveryStart(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 启动局域网发现。
		const int code = handle_->DiscoveryStart(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::DiscoveryStop() {
		SdkBuffer output(library_.Free);
		// 停止局域网发现。
		const int code = handle_->DiscoveryStop();
		return MakeResponse(code, output);
	}

	Json Sdk::DiscoveryStatus() {
		SdkBuffer output(library_.Free);
		// 查询发现服务状态。
		const int code = handle_->DiscoveryStatus(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::DiscoveryList() {
		SdkBuffer output(library_.Free);
		// 列出已发现的设备。
		const int code = handle_->DiscoveryList(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::ProximityInvitationCreate(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 创建近距离邀请。
		const int code = handle_->ProximityInvitationCreate(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::ProximityInvitationInspect(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 检查邀请内容。
		const int code = handle_->ProximityInvitationInspect(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::PairingStartFromInvitation(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 根据邀请发起配对。
		const int code = handle_->PairingStartFromInvitation(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::BleTransport(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交宿主提供的 BLE 传输操作（非虚 C 辅助入口）。
		const int code = library_.BleTransport(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::PairingStart(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 发起设备配对。
		const int code = handle_->PairingStart(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::PairingStatus() {
		SdkBuffer output(library_.Free);
		// 查询配对状态。
		const int code = handle_->PairingStatus(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::SessionList() {
		SdkBuffer output(library_.Free);
		// 列出会话。
		const int code = handle_->SessionList(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::SessionResume(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 恢复指定会话。
		const int code = handle_->SessionResume(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::WanOfferCreate(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 创建 WAN 连接提议。
		const int code = handle_->WanOfferCreate(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::WanOfferAccept(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 接受 WAN 连接提议。
		const int code = handle_->WanOfferAccept(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::WanAnswerAccept(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 接受 WAN 连接应答。
		const int code = handle_->WanAnswerAccept(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::NetworkPathList() {
		SdkBuffer output(library_.Free);
		// 列出网络路径（非虚 C 辅助入口）。
		const int code = library_.NetworkPathList(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::NetworkPathsUpdate(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 更新宿主提供的网络路径（非虚 C 辅助入口）。
		const int code = library_.NetworkPathsUpdate(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::WanRouteList() {
		SdkBuffer output(library_.Free);
		// 列出 WAN 路由。
		const int code = handle_->WanRouteList(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::WanRouteStop(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 停止指定 WAN 路由。
		const int code = handle_->WanRouteStop(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::PairingConfirm(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 明确确认或拒绝配对。
		const int code = handle_->PairingConfirm(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::PairingCancel() {
		SdkBuffer output(library_.Free);
		// 取消当前配对。
		const int code = handle_->PairingCancel();
		return MakeResponse(code, output);
	}

	Json Sdk::RelationshipList() {
		SdkBuffer output(library_.Free);
		// 列出已建立的信任关系。
		const int code = handle_->RelationshipList(&output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::RelationshipRemove(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 删除指定信任关系。
		const int code = handle_->RelationshipRemove(input.data(), input.size());
		return MakeResponse(code, output);
	}

	Json Sdk::NetworkShare(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交用户选择的网络共享操作（非虚 C 辅助入口）。
		const int code = library_.NetworkShare(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

	Json Sdk::ConnectionAssistance(const std::string& input) {
		SdkBuffer output(library_.Free);
		// 提交连接辅助操作（非虚 C 辅助入口）。
		const int code = library_.ConnectionAssistance(input.data(), input.size(), &output.data, &output.size);
		return MakeResponse(code, output);
	}

}
