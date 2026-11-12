#pragma once
#include <cstdio>
#include <string>
#include <tuple>

// Parses "1.2.3" or "v1.2.3". The text may come from the network, so anything
// unexpected simply fails to parse, and each number is capped at 4 digits.
struct Version
{
    int major = 0, minor = 0, patch = 0;
    bool valid = false;

    explicit Version (const std::string& text)
    {
        const char* start = text.c_str();
        if (*start == 'v')
            ++start;
        valid = std::sscanf (start, "%4d.%4d.%4d", &major, &minor, &patch) == 3 && major >= 0 && minor >= 0 && patch >= 0;
    }

    bool newerThan (const Version& other) const
    {
        return valid && other.valid && std::tie (major, minor, patch) > std::tie (other.major, other.minor, other.patch);
    }
};
