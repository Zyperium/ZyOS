#include <Library/path.hpp>

namespace lib {
    // Note: EXPECTS an A:/path/to/file OR an /A/path/to/file format.
    fullpath parse_path(string s) {
        fullpath path;
        if (s.length() < MIN_PATH_LENGTH)
            return path;

        if (s[0] == '/') {
            path.drv = s[1];
        }
        else {
            path.drv = s[0];
        }
        
        s = s.substr(MIN_PATH_LENGTH);

        if (s.length() == 0) {
            path.path = "/";
            return path;
        }

        path.path = s;
        return path;
    }
}