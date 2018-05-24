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

using asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant {
public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;

  virtual const char* from() = 0;
  virtual const char* to() = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room {
public:
  void join(chat_participant_ptr participant) { participants_.insert(participant); }
  void leave(chat_participant_ptr participant) { participants_.erase(participant); }

  void deliver(const chat_message& msg) {
    for (auto participant : participants_) {
      if (std::strcmp(msg.from(), participant->to()) == 0) {
        if (std::strcmp(msg.to(), participant->from()) == 0) {
          participant->deliver(msg);
        }
      }
    }
  }

private:
  std::set<chat_participant_ptr> participants_;
};

//----------------------------------------------------------------------

class chat_session : public chat_participant, public std::enable_shared_from_this<chat_session> {
public:
  chat_session(tcp::socket socket, chat_room& room) : socket_(std::move(socket)), room_(room) {}

  void start() {
    room_.join(shared_from_this());
    do_read_header();
  }

  void deliver(const chat_message& msg) override {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
      do_write();
    }
  }

  const char* from() override { return from_; }
  const char* to() override { return to_; }

private:
  void do_read_header() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(read_msg_.data(), chat_message::header_length),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec && read_msg_.unpack()) {
                         do_read_from();
                       } else {
                         room_.leave(shared_from_this());
                       }
                     });
  }

  void do_read_from() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(read_msg_.from(), read_msg_.from_length()),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         from_ = read_msg_.from();
                         do_read_to();
                       } else {
                         room_.leave(shared_from_this());
                       }
                     });
  }

  void do_read_to() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(read_msg_.to(), read_msg_.to_length()),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         to_ = read_msg_.to();
                         do_read_body();
                       } else {
                         room_.leave(shared_from_this());
                       }
                     });
  }

  void do_read_body() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(read_msg_.body(), read_msg_.body_length()),
                     [this, self](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         read_msg_.pack();
                         room_.deliver(read_msg_);
                         do_read_header();
                       } else {
                         room_.leave(shared_from_this());
                       }
                     });
  }

  void do_write() {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                          write_msgs_.pop_front();
                          if (!write_msgs_.empty()) {
                            do_write();
                          }
                        } else {
                          room_.leave(shared_from_this());
                        }
                      });
  }

  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;

  const char* from_;
  const char* to_;
};

//----------------------------------------------------------------------

class chat_server {
public:
  chat_server(asio::io_context& io_context, const tcp::endpoint& endpoint) : acceptor_(io_context, endpoint) {
    do_accept();
  }

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
      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }

    asio::io_context io_context;

    std::list<chat_server> servers;
    for (int i = 1; i < argc; ++i) {
      tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
      servers.emplace_back(io_context, endpoint);
    }

    io_context.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
