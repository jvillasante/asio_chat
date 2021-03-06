#include "asio.hpp"
#include "chat_message.hpp"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <sstream>

using asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant {
public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;

  virtual const char* name() = 0;
  virtual const tcp::socket& socket() = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room {
public:
  void join(chat_participant_ptr participant) {
    auto rc = participants_.insert(participant);
    if (rc.second) {
      chat_message response = user_joined_message(participant->name());
      broadcast(response);
    }
  }

  void leave(chat_participant_ptr participant) {
    chat_message response = user_left_message(participant->name());
    broadcast(response);

    participants_.erase(participant);
  }

  std::string get_participants() {
    std::stringstream ss;
    for (const auto& participant : participants_) {
      const auto& remote = participant->socket().remote_endpoint();

      ss << participant->name();
      ss << " @ ";
      ss << remote.address().to_string();
      ss << ":";
      ss << remote.port();
      ss << "\n";
    }

    return ss.str();
  }

  void deliver(const chat_message& msg) {
    std::string body(msg.body());
    if (body.empty()) {
      return;
    } else if (body[0] != '/') { // Room message
      chat_message response = room_message(msg);
      broadcast(response);
    } else if (body[0] == '/') {              // Command message
      std::string command = body.erase(0, 1); // Remove / character

      if (command == "users") { // Handle command for user list
        auto participant = find_participant(msg.name());
        if (participant) {
          chat_message response = user_list_message(msg);
          participant->deliver(response);
        }
      } else { // Handle command for private message
        std::string user = {};
        std::size_t space_pos = command.find(" ");
        if (space_pos != std::string::npos) {
          user = command.substr(0, space_pos);
        }

        auto participant = find_participant(user.c_str());
        if (participant) {
          chat_message response = private_message(msg);
          participant->deliver(response);
        }
      }
    }
  }

private:
  chat_participant_ptr find_participant(const char* name) {
    auto participant_iter = std::find_if(
        participants_.begin(), participants_.end(),
        [&name](const auto& p) { return (std::strcmp(p->name(), name) == 0); });

    if (participant_iter != participants_.end()) {
      return *participant_iter;
    }

    return nullptr;
  }

  void broadcast(const chat_message& msg) {
    for (auto participant : participants_) {
      if (std::strcmp(msg.name(), participant->name()) != 0) {
        participant->deliver(msg);
      }
    }
  }

  chat_message user_left_message(const char* name) {
    std::stringstream ss;
    ss << name;
    ss << " left the chat.";
    return build_message(name, ss.str().c_str());
  }

  chat_message user_joined_message(const char* name) {
    std::stringstream ss;
    ss << name;
    ss << " joined the chat.";
    return build_message(name, ss.str().c_str());
  }

  chat_message room_message(const chat_message& from_msg) {
    std::stringstream ss;
    ss << from_msg.name() << " says: ";
    ss << from_msg.body();
    return build_message(from_msg.name(), ss.str().c_str());
  }

  chat_message user_list_message(const chat_message& from_msg) {
    std::stringstream ss;
    for (const auto& participant : participants_) {
      if (std::strcmp(participant->name(), from_msg.name()) != 0) {
        ss << participant->name();
        ss << "\n";
      }
    }

    std::string users = ss.str();
    if (users.empty()) {
      return build_message(from_msg.name(), "No users connected.\n");
    } else {
      return build_message(from_msg.name(), users.c_str());
    }
  }

  chat_message private_message(const chat_message& from_msg) {
    std::string body(from_msg.body());

    std::size_t space_pos = body.find(" ");
    if (space_pos != std::string::npos) {
      body = body.substr(space_pos + 1);
    }

    std::stringstream ss;
    ss << from_msg.name() << " says: ";
    ss << body;
    return build_message(from_msg.name(), ss.str().c_str());
  }

  chat_message build_message(const char* name, const char* body) {
    chat_message msg;

    msg.name_length(std::strlen(name));
    std::memcpy(msg.name(), name, msg.name_length());

    msg.body_length(std::strlen(body));
    std::memcpy(msg.body(), body, msg.body_length());

    msg.pack();
    return msg;
  }

private:
  std::set<chat_participant_ptr> participants_;
};

//----------------------------------------------------------------------

class chat_session : public chat_participant,
                     public std::enable_shared_from_this<chat_session> {
public:
  chat_session(tcp::socket socket, chat_room& room)
      : socket_(std::move(socket)), room_(room) {}

  void start() { do_read_header(); }

  void deliver(const chat_message& msg) override {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      do_write();
    }
  }

  const char* name() override { return name_; }
  const tcp::socket& socket() override { return socket_; }

private:
  void do_read_header() {
    auto self(shared_from_this());
    asio::async_read(
        socket_, asio::buffer(read_msg_.data(), chat_message::header_length),
        [this, self](std::error_code ec, std::size_t /*length*/) {
          if (!ec && read_msg_.unpack()) {
            do_read_name();
          } else {
            room_.leave(shared_from_this());
          }
        });
  }

  void do_read_name() {
    auto self(shared_from_this());
    asio::async_read(socket_,
                     asio::buffer(read_msg_.name(), read_msg_.name_length()),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         read_msg_.name()[read_msg_.name_length()] = '\0';
                         name_ = read_msg_.name();
                         room_.join(self); // We need a name before joining
                         do_read_body();
                       } else {
                         room_.leave(self);
                       }
                     });
  }

  void do_read_body() {
    auto self(shared_from_this());
    asio::async_read(socket_,
                     asio::buffer(read_msg_.body(), read_msg_.body_length()),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         read_msg_.body()[read_msg_.body_length()] = '\0';
                         read_msg_.pack();
                         room_.deliver(read_msg_);
                         do_read_header();
                       } else {
                         room_.leave(self);
                       }
                     });
  }

  void do_write() {
    auto self(shared_from_this());
    asio::async_write(
        socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this, self](std::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              do_write();
            }
          } else {
            room_.leave(self);
          }
        });
  }

  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
  const char* name_;
};

//----------------------------------------------------------------------

class chat_server {
public:
  chat_server(asio::io_context& io_context, const tcp::endpoint& endpoint)
      : acceptor_(io_context, endpoint) {
    do_accept();
  }

  std::string get_participants() { return room_.get_participants(); }

private:
  void do_accept() {
    acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
      if (!ec) {
        std::make_shared<chat_session>(std::move(socket), room_)->start();
      }

      do_accept();
    });
  }

  tcp::acceptor acceptor_;
  chat_room room_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[]) {
  try {
    if (argc < 2) {
      std::cerr << "Usage: chat_server <port>\n";
      return 1;
    }

    asio::io_context io_context;

    tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));
    chat_server server(io_context, endpoint);

    std::thread t([&io_context]() { io_context.run(); });

    char line[32];
    while (std::cin.getline(line, chat_message::max_body_length + 1)) {
      if (std::strcmp(line, "/users") == 0) {
        auto participants = server.get_participants();
        if (participants.empty()) {
          std::cout << "No users connected.\n";
        } else {
          std::cout << server.get_participants();
        }
      }
    }

    t.join();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
