#pragma once
#include "sdk.hpp"
namespace devtools {
struct Recipe { std::string group, title, help; Json request = Json::object(); bool confirm = false; };
Recipe recipe(const std::string &op);
}
