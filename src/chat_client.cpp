#include "chat_message.hpp"
#include <asio.hpp>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <thread>

using asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

class chat_client {
public:
  chat_client(asio::io_context& io_context,
              const tcp::resolver::results_type& endpoints, const char* name)
      : io_context_(io_context), socket_(io_context), name_(name) {
    do_connect(endpoints);
  }

  void write(const chat_message& msg) {
    asio::post(io_context_, [this, msg]() {
      bool write_in_progress = !write_msgs_.empty();
      write_msgs_.push_back(msg);
      if (!write_in_progress) {
        do_write();
      }
    });
  }

  void close() {
    asio::post(io_context_, [this]() { socket_.close(); });
  }

private:
  void do_connect(const tcp::resolver::results_type& endpoints) {
    asio::async_connect(socket_, endpoints,
                        [this](std::error_code ec, tcp::endpoint) {
                          if (!ec) {
                            do_read_header();
                          }
                        });
  }

  void do_read_header() {
    asio::async_read(
        socket_, asio::buffer(read_msg_.data(), chat_message::header_length),
        [this](std::error_code ec, std::size_t /*length*/) {
          if (!ec && read_msg_.unpack()) {
            do_read_name();
          } else {
            socket_.close();
          }
        });
  }

  void do_read_name() {
    asio::async_read(socket_,
                     asio::buffer(read_msg_.name(), read_msg_.name_length()),
                     [this](std::error_code ec, std::size_t /*length*/) {
                       if (!ec) {
                         read_msg_.name()[read_msg_.name_length()] = '\0';
                         name_ = read_msg_.name();
                         do_read_body();
                       } else {
                         socket_.close();
                       }
                     });
  }

  void do_read_body() {
    asio::async_read(
        socket_, asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](std::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            read_msg_.body()[read_msg_.body_length()] = '\0';
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "\n";

            do_read_header();
          } else {
            socket_.close();
          }
        });
  }

  void do_write() {
    asio::async_write(
        socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this](std::error_code ec, std::size_t /*length*/) {
          if (!ec) {
            write_msgs_.pop_front();
            if (!write_msgs_.empty()) {
              do_write();
            }
          } else {
            socket_.close();
          }
        });
  }

private:
  asio::io_context& io_context_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
  const char* name_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc != 3) {
      std::cerr << "Usage: chat_client <host> <port>\n";
      return 1;
    }

    // Get name
    std::cout << "Enter a username: ";
    char name[chat_message::max_name_length + 1];
    std::cin >> name;

    // Create Client
    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(argv[1], argv[2]);
    chat_client c(io_context, endpoints, name);

    std::thread t([&io_context]() { io_context.run(); });

    // Chat!!
    char line[chat_message::max_body_length + 1];
    while (std::cin.getline(line, chat_message::max_body_length + 1)) {
      chat_message msg;

      msg.name_length(std::strlen(name));
      msg.set_name(name);

      msg.body_length(std::strlen(line));
      msg.set_body(line);

      msg.pack();
      c.write(msg);

      if (std::strcmp(msg.body(), "/quit") == 0) {
        break;
      }
    }

    c.close();
    t.join();
    std::cout << "Bye Bye!!!\n";
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
