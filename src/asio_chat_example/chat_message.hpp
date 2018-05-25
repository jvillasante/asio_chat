#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

class chat_message {
public:
  enum { header_length = 12 };
  enum { header_from_length = 4 };
  enum { header_to_length = 4 };
  enum { header_body_length = 4 };
  enum { max_from_length = 256 };
  enum { max_to_length = 256 };
  enum { max_body_length = 512 };

  chat_message() : body_length_(0), from_length_(0), to_length_(0) {}

  // Data
  const char* data() const { return data_; }
  char* data() { return data_; }

  std::size_t length() const {
    return header_length + body_length_ + from_length_ + to_length_;
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

  // From
  const char* from() const { return from_; }
  char* from() { return from_; }

  std::size_t from_length() const { return from_length_; }

  void from_length(std::size_t new_length) {
    from_length_ = new_length;
    if (from_length_ > max_from_length)
      from_length_ = max_from_length;
  }

  // To
  const char* to() const { return to_; }
  char* to() { return to_; }

  std::size_t to_length() const { return to_length_; }

  void to_length(std::size_t new_length) {
    to_length_ = new_length;
    if (to_length_ > max_to_length)
      to_length_ = max_to_length;
  }

  // Pack / Unpack message
  void pack() {
    memset(data_, 0,
           header_length + max_from_length + max_to_length + max_body_length);
    char header[header_length + 1] = "";
    std::sprintf(header, "%4d%4d%4d", static_cast<int>(from_length_),
                 static_cast<int>(to_length_), static_cast<int>(body_length_));
    std::memcpy(data_, header, header_length);
    std::strncat(data_, from_, from_length_);
    std::strncat(data_, to_, to_length_);
    std::strncat(data_, body_, body_length_);
  }

  bool unpack() {
    char from_length_buffer[header_from_length + 1] = "";
    std::strncat(from_length_buffer, data_, header_from_length);
    from_length_ = std::atoi(from_length_buffer);
    if (from_length_ > max_from_length) {
      from_length_ = 0;
      return false;
    }

    char to_length_buffer[header_to_length + 1] = "";
    std::strncat(to_length_buffer, data_ + header_from_length,
                 header_to_length);
    to_length_ = std::atoi(to_length_buffer);
    if (to_length_ > max_to_length) {
      to_length_ = 0;
      return false;
    }

    char body_length_buffer[header_body_length + 1] = "";
    std::strncat(body_length_buffer,
                 data_ + header_from_length + header_to_length,
                 header_body_length);
    body_length_ = std::atoi(body_length_buffer);
    if (body_length_ > max_body_length) {
      body_length_ = 0;
      return false;
    }

    return true;
  }

private:
  char data_[header_length + max_from_length + max_to_length + max_body_length];
  char from_[max_from_length];
  char to_[max_to_length];
  char body_[max_body_length];
  std::size_t body_length_, from_length_, to_length_;
};

#endif // CHAT_MESSAGE_HPP
