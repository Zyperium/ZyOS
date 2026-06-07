#include <Library/cystr.hpp>
#include <Library/string.h>

namespace lib {
    
    bool string::is_local() const {
        return (local.info <= SSO_CAPACITY);
    }

    string::string() {
        local.data[0] = '\0';
        local.info = 0;
    }

    string::string(const char *s) {
        if (!s) {
            local.data[0] = '\0';
            local.info = 0;
            return;
        }

        size_t len = strlen(s);

        if (len <= SSO_CAPACITY) {
            memcpy(local.data, s, len);
            local.data[len] = '\0';
            local.info = (unsigned char)len;
        } else {
            remote.data = new char[len + 1];
            memcpy(remote.data, s, len);
            remote.data[len] = '\0';
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    }

    string::~string() {
        if (!is_local()) delete[] remote.data;
    }

    string::string(const string& other) {
        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            size_t len = other.remote.size;
            remote.data = new char[len + 1];
            memcpy(remote.data, other.remote.data, len + 1);
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    }

    string::string(string&& other) noexcept {
        memcpy(this, &other, sizeof(string));
        other.local.data[0] = '\0';
        other.local.info = 0; 
    }

    string& string::operator=(const string& other) {
        if (this == &other) return *this;

        if (!is_local()) delete[] remote.data;

        if (other.is_local()) {
            memcpy(local.data, other.local.data, SSO_CAPACITY + 1);
            local.info = other.local.info;
        } else {
            size_t len = other.remote.size;
            remote.data = new char[len + 1];
            memcpy(remote.data, other.remote.data, len + 1);
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
        return *this;
    }

    string& string::operator=(const char* s) {
        if (!is_local()) {
            delete[] remote.data;
        }
    
        if (s == nullptr) {
            local.data[0] = '\0';
            local.info = 0;
            return *this;
        }
    
        size_t len = strlen(s);
    
        if (len <= SSO_CAPACITY) {
            memcpy(local.data, s, len);
            local.data[len] = '\0';
            local.info = (unsigned char)len;
        } else {
            remote.data = new char[len + 1];
            memcpy(remote.data, s, len);
            remote.data[len] = '\0';
            remote.size = len;
            remote.capacity = len;
            local.info = 0xFF;
        }
    
        return *this;
    }

    string& string::operator=(string&& other) noexcept {
        if (this == &other) return *this;

        if (!is_local()) delete[] remote.data;

        memcpy(this, &other, sizeof(string));
        
        other.local.data[0] = '\0';
        other.local.info = 0;
        
        return *this;
    }

    const char* string::c_str() const {
        return is_local() ? local.data : remote.data;
    }

    char& string::operator[](size_t index) {
        return is_local() ? local.data[index] : remote.data[index];
    }

    const char& string::operator[](size_t index) const {
        return is_local() ? local.data[index] : remote.data[index];
    }

    size_t string::length() const {
        return is_local() ? local.info : remote.size;
    }

    bool string::empty() const {
        return length() == 0;
    }

    void string::clear() {
        if (!is_local()) {
            delete[] remote.data;
        }
        local.data[0] = '\0';
        local.info = 0;
    }

    bool lib::string::operator==(const char* other) const {
        if (!other) return empty();
        return strcmp(c_str(), other) == 0;
    }

    bool lib::string::operator==(const string& other) const {
        if (length() != other.length()) return false;
        return strcmp(c_str(), other.c_str()) == 0;
    }

    bool lib::string::operator!=(const char* other) const {
        return !(*this == other);
    }

    bool lib::string::operator!=(const string& other) const {
        return !(*this == other);
    }
}