#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

class chat_message {
public:
  enum { header_length = 8 };
  enum { header_name_length = 4 };
  enum { header_body_length = 4 };
  enum { max_name_length = 256 };
  enum { max_body_length = 512 };

  chat_message() : body_length_(0), name_length_(0) {}

  // Data
  const char* data() const { return data_; }
  char* data() { return data_; }

  std::size_t length() const {
    return header_length + body_length_ + name_length_;
  }

  // Body
  const char* body() const { return body_; }
  char* body() { return body_; }

  std::size_t body_length() const { return body_length_; }

  void body_length(std::size_t new_length) {
    body_length_ = new_length;
    if (body_length_ > max_body_length)
      body_length_ = max_body_length;
  }

  // Name
  const char* name() const { return name_; }
  char* name() { return name_; }

  std::size_t name_length() const { return name_length_; }

  void name_length(std::size_t new_length) {
    name_length_ = new_length;
    if (name_length_ > max_name_length)
      name_length_ = max_name_length;
  }

  // Pack / Unpack message
  void pack() {
    memset(data_, 0, header_length + max_name_length + max_body_length);
    char header[header_length + 1] = "";
    std::sprintf(header, "%4d%4d", static_cast<int>(name_length_),
                 static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);
    std::strncat(data_, name_, name_length_);
    std::strncat(data_, body_, body_length_);
  }

  bool unpack() {
    char name_length_buffer[header_name_length + 1] = "";
    std::strncat(name_length_buffer, data_, header_name_length);
    name_length_ = std::atoi(name_length_buffer);
    if (name_length_ > max_name_length) {
      name_length_ = 0;
      return false;
    }

    char body_length_buffer[header_body_length + 1] = "";
    std::strncat(body_length_buffer, data_ + header_name_length,
                 header_body_length);
    body_length_ = std::atoi(body_length_buffer);
    if (body_length_ > max_body_length) {
      body_length_ = 0;
      return false;
    }

    return true;
  }

private:
  char data_[header_length + max_name_length + max_body_length];
  char name_[max_name_length];
  char body_[max_body_length];
  std::size_t body_length_, name_length_;
};

#endif // CHAT_MESSAGE_HPP
