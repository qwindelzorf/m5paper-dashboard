#include "config_file.h"

using namespace std;

bool has_key(const std::string& key, const std::map<std::string, std::string>& config)
{
    if (config.find(key) == config.end())
        return false;
    return true;
}
std::string value_or(const std::string& key, const std::map<std::string, std::string>& config, std::string default_val)
{
    if (!has_key(key, config))
        return default_val;
    return config.at(key);
}

std::map<string, string> read_file_to_map(File& in_file)
{
    std::map<string, string> kv_pairs;
    while (in_file.available()) {
        auto k = rtrim_copy(string(in_file.readStringUntil(':').c_str()));
        auto v = rtrim_copy(string(in_file.readStringUntil('\n').c_str()));
        kv_pairs[k] = v;
    }
    return kv_pairs;
}

// trim from start (in place)
static void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(),
        s.end());
}

// trim from both ends (in place)
static void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static std::string ltrim_copy(std::string s)
{
    ltrim(s);
    return s;
}

// trim from end (copying)
static std::string rtrim_copy(std::string s)
{
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static std::string trim_copy(std::string s)
{
    trim(s);
    return s;
}