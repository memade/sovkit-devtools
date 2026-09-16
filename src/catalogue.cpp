#include "catalogue.hpp"
namespace devtools {
Recipe recipe(const std::string &op) {
  Recipe r{"其他 API", op, "输入 JSON 对象；响应 code=0 不一定表示异步业务已完成。详细合同见随包 SDK 接入文档。"};
  if (op.starts_with("identity")) r.group = "身份与设备";
  if (op.starts_with("discovery") || op.starts_with("proximity")) r.group = "局域网发现与邀请";
  if (op.starts_with("pairing") || op.starts_with("session") || op.starts_with("relationship")) r.group = "配对与信任";
  if (op.starts_with("message") || op == "conversation_export") r.group = "消息";
  if (op.starts_with("transfer")) r.group = "文件传输";
  if (op.starts_with("log")) r.group = "运行日志";
  if (op.starts_with("store")) r.group = "加密业务库";
  if (op.starts_with("wan") || op.starts_with("network") || op == "connection_assistance") r.group = "网络实验 · 手动";
  if (op == "ble_transport") { r.group = "BLE 帧调试"; r.help = "这是 SDK 帧接口，不是系统蓝牙扫描器。桌面工具未接入 GATT 驱动；可手动注入/提取帧验证协议。"; r.request = {{"op", "poll"}}; }
  if (op == "info" || op == "selftest" || op == "transport_probe") r.group = "SDK 检查";
  if (op == "selftest") { r.title = "ABI / 内存与加密自检"; r.help = "检查全部动态符号和二进制加解密往返，不创建身份或发送网络流量。"; }
  if (op == "info") { r.title = "版本与能力"; r.help = "显示运行库的 sdkVersion、ABI 和能力；不能从头文件推断运行库功能。"; }
  if (op == "discovery_start") { r.title = "开始局域网发现"; r.request = {{"candidateTtlMs", 12000}}; r.help = "先启动测试身份；发现是候选，不会自动建立信任。可添加 listenPort 固定会话端口。"; }
  if (op == "discovery_list") r.title = "查看附近设备";
  if (op == "pairing_start") { r.request = {{"address", "127.0.0.1"}, {"pairingPort", 0}}; r.help = "填入 discovery_list 返回的 address / pairingPort。双方须分别检查安全码并确认。"; }
  if (op == "pairing_confirm") { r.request = {{"accept", true}}; r.confirm = true; r.help = "先查看 pairing_status 并与对方核对安全码，双方分别确认。"; }
  if (op == "message_send") { r.request = {{"relationshipId", ""}, {"text", "你好，SovKit"}}; r.help = "从 relationship_list 复制关系 ID。返回成功是入队，送达看事件和消息状态。"; }
  if (op == "message_list" || op == "conversation_export" || op == "relationship_remove" || op == "session_resume") r.request = {{"relationshipId", ""}};
  if (op == "transfer_offer") { r.request = {{"relationshipId", ""}, {"sourcePaths", Json::array()}, {"logicalNames", Json::array()}, {"asyncPreparation", true}}; r.help = "选择文件按钮填入路径；指定关系 ID。局域网传文件，BLE 不支持文件传输。"; }
  if (op == "transfer_decide") { r.request = {{"transferId", ""}, {"accept", true}, {"destinationDirectory", ""}}; r.help = "先查看 transfer_list，再明确接受或拒绝；接受时选择接收目录。"; r.confirm = true; }
  if (op.starts_with("transfer_") && op != "transfer_offer" && op != "transfer_decide" && op != "transfer_list" && op != "transfer_flush") r.request = {{"transferId", ""}};
  if (op == "identity_create" || op == "identity_rename_device") r.request = {{"deviceName", "DevTools"}};
  if (op == "proximity_invitation_inspect" || op == "pairing_start_from_invitation") r.request = {{"uri", ""}};
  if (op == "network_share" || op == "connection_assistance") r.request = {{"op", "status"}};
  if (op == "network_paths_update") r.request = {{"paths", Json::array()}};
  if (op == "wan_offer_create") r.request = {{"step", "begin"}, {"purpose", "pairing"}};
  if (op == "wan_offer_accept" || op == "wan_answer_accept") r.request = {{"package", ""}};
  if (op == "wan_route_stop") r.request = {{"generation", ""}};
  if (op == "store_query_v1") { r.request = {{"kind", "message"}, {"limit", 50}}; r.help = "需要持久化测试身份打开 SQLCipher。返回 requestId 后用 store_result_v1 查询实际完成状态。kind 支持 message / transfer。"; }
  if (op == "store_result_v1") { r.request = {{"requestId", 0}}; r.help = "填入提交响应中的数值 requestId；未完成为 -10004，完成后仍检查 result.status。"; }
  if (op == "log_configure_v1") { r.request = {{"version", 1}, {"directory", ""}, {"role", "helper"}, {"console", false}, {"minimumLevel", "info"}}; r.help = "先填写独立日志目录的绝对路径。日志是脱敏明文，不是密码库内容；role 使用 helper。"; }
  if (op == "log_read_v1") r.request = {{"afterSeq", "0"}, {"maxCount", 100}};
  if (op == "identity_export" || op == "identity_import" || op.find("remove") != std::string::npos || op.find("revoke") != std::string::npos || op.find("forget") != std::string::npos || r.group == "网络实验 · 手动") r.confirm = true;
  return r;
}
}
