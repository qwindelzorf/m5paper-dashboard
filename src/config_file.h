#include <FS.h>
#include <map>
#include <string>

static void ltrim(std::string& s);
static void rtrim(std::string& s);
static void trim(std::string& s);
static std::string ltrim_copy(std::string s);
static std::string rtrim_copy(std::string s);
static std::string trim_copy(std::string s);

std::map<std::string, std::string> read_file_to_map(File& in_file);
bool has_key(const std::string& key, const std::map<std::string, std::string>& config);
std::string value_or(const std::string& key, const std::map<std::string, std::string>& config, std::string default_val);
