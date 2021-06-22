#pragma once

#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <cerrno>

// Godot includes
#include <Godot.hpp>
#include <Node.hpp>
#include <Array.hpp>
#include <String.hpp>

// cppzmq includes
#include <zmq.hpp> 

// GodotAiBridge includes
#include "util.h"
#include "msg.h"

namespace gab {
	static const int DEFAULT_PUBLISHER_PORT = 10001;
	static const int DEFAULT_LISTENER_PORT = 10002;

	// forward declarations
	class GodotAiBridge;
	class Listener;
	class Publisher;

	class Listener {
	private:
		zmq::socket_t* p_socket;  // ZeroMq socket backing this connection
		uint64_t seq_no;  // event sequence numbers
		uint16_t port;

		GodotAiBridge& bridge;  // used to communicate with Godot engine (e.g., sending signals)

		zmq::message_t create_reply(const uint64_t seq_no, const std::string& parse_errors);
	public:

		Listener(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port, GodotAiBridge& bridge);

		void operator()();
		void receive(const zmq::message_t& request);
	};

	class Publisher {
	private:
		zmq::socket_t* p_socket;  // ZeroMq socket backing this connection
		uint16_t port;
		uint64_t seq_no;  // publisher message sequence numbers

	public:

		// TODO: change port to const std::string& endpoint
		Publisher(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port);

		void publish(const std::string& topic, const std::string& content);
		uint64_t get_seq_no();
	};

	class GodotAiBridge : public godot::Node {
		GODOT_CLASS(GodotAiBridge, Node);

	private:

		// zmq context used for all connections
		zmq::context_t zmq_context;

		Listener* p_listener;
		Publisher* p_publisher;

		std::thread* p_listener_thread;  // a thread for listener's receive loop

	public:

		GodotAiBridge();
		~GodotAiBridge();

		// GDNative required methods
		static void _register_methods();
		void _init();

		// GDNative exposed methods
		void connect(godot::Variant v_options);
		void send(const godot::Variant v_topic, const godot::Variant v_content);
		void notify(const zmq::message_t& request, std::string& parse_errors);
	};

	inline void set_options(zmq::socket_t& socket, std::map<int, int>& socket_options) {
		for (std::map<int, int>::iterator it = socket_options.begin(); it != socket_options.end(); ++it) {
			zmq_setsockopt(socket, it->first, &it->second, sizeof(it->second));
		}
	}

	inline std::string construct_endpoint(int port) {
		return "tcp://*:" + std::to_string(port);
	}
};