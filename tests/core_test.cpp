#include "sdk.hpp"
#include <iostream>
#include <stdexcept>
using namespace devtools;
void require(bool value, const char* text) {
	if (!value)
		throw std::runtime_error(text);
}
int main(int argc, char** argv) {
	try {
		require(argc == 2, "library argument");
		Json private_record{{"operation", "message_send"}, {"code", 0}, {"elapsedMs", 2}, {"password", "SECRET"}, {"data", {{"text", "SECRET"}, {"path", "/private/location"}}}};
		auto safe = diagnostic(private_record).dump();
		require(safe.find("SECRET") == std::string::npos && safe.find("/private") == std::string::npos, "diagnostic leakage");
		require(!diagnostic({{"operation", "untrusted SECRET"}}).contains("operation"), "untrusted operation leakage");
		Sdk sdk;
		require(!sdk.loaded() && sdk.handle() == nullptr, "construction must not load SDK");
		require(sdk.execute("stop")["code"] == 0, "close before loading SDK");
		bool rejected = false;
		try {
			sdk.load(fs::path("relative-sdk.dll"));
		}
		catch (const std::exception&) {
			rejected = true;
		}
		require(rejected && !sdk.loaded(), "reject relative library without keeping partial state");
		sdk.load(path_from_utf8(argv[1]));
		require(sdk.handle() != nullptr && !sdk.handle()->IsRunning(), "InitCpp must obtain a borrowed handle without starting SDK");
		ISovKit* const handle = sdk.handle();
		require(operations().size() == 58, "complete operation catalogue");
		require(sdk.execute("info")["code"] == 0, "info");
		require(sdk.execute("selftest")["code"] == 0, "binary roundtrip");
		auto started = sdk.start({}, "", "DevTools test");
		require(started["code"] == 0, "start");
		require(handle->IsRunning(), "C++ handle reports running after Init");
		require(sdk.execute("identity_open")["code"] == 0, "identity");
		require(sdk.execute("discovery_status")["code"] == 0, "discovery status");
		auto invalid = sdk.execute("pairing_start", {{"address", "127.0.0.1"}, {"pairingPort", 0}});
		require(invalid["code"] == -10000 && invalid["data"].is_null(), "error ownership");
		require(sdk.stop()["code"] == 0, "stop");
		require(!handle->IsRunning() && sdk.handle() == handle, "Shutdown retains the borrowed C++ handle");
		require(sdk.start({}, "", "Second session")["code"] == 0, "restart");
		require(sdk.stop()["code"] == 0, "second stop");
		std::cout << "PASS: dlfcn loader, C++ handle lifecycle, ABI, memory ownership, report redaction\n";
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
