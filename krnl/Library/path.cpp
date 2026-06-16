#include <Library/path.hpp>

namespace lib {
    fullpath parse_path(string s) {
        fullpath path;
        if (s.length() < MIN_PATH_LENGTH)
            return path;

        path.drv = s[0];
        s = s.substr(MIN_PATH_LENGTH);

        if (s.length() == 0) {
            path.path = "/";
            return path;
        }

        path.path = s;
        return path;
    }
}