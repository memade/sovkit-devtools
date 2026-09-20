#include "sdk.hpp"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
#else
int main(int argc, char** argv) {
#endif
	using namespace devtools;
	Sdk sdk;
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: sovkit-console /absolute/path/libsovkit.{dylib,so,dll}\nJSONL requests on stdin: {\"id\":1,\"op\":\"info\",\"request\":{}}\n";
			return 64;
		}
#ifdef _WIN32
		sdk.load(fs::path(argv[1])); // Windows argv is UTF-16; JSONL remains UTF-8.
#else
		sdk.load(path_from_utf8(argv[1]));
#endif
		std::string line;
		while (std::getline(std::cin, line))
		{
			Json reply;
			try
			{
				if (line.size() > 1024 * 1024)
					throw std::runtime_error("Request exceeds 1 MiB");
				auto request = Json::parse(line);
				wipe(line.data(), line.size());
				reply["id"] = request.value("id", Json(nullptr));
				auto op = request.at("op").get<std::string>();
				if (op == "start")
					reply["result"] = sdk.start(path_from_utf8(request.value("profile", "")), request.value("password", ""), request.value("deviceName", "DevTools"));
				else
					reply["result"] = sdk.execute(op, request.value("request", Json::object()));
			}
			catch (const std::exception& error)
			{ reply["result"] = {{"code", -1}, {"message", error.what()}}; }
			std::cout << reply.dump() << std::endl;
		}
		auto stopped = sdk.stop();
		return stopped["code"] == 0 ? 0 : 2;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
