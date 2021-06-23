#pragma once

#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <cerrno>
#include <chrono>

// Godot includes
#include <Godot.hpp>
#include <Node.hpp>
#include <Array.hpp>
#include <String.hpp>

// cppzmq includes
#include <zmq.hpp> 

// GodotAiBridge includes
#include "util.h"
//#include "msg.h"

namespace gab {

	using json = nlohmann::json;

	// forward declarations
	class GodotAiBridge;
	class Listener;
	class Publisher;

	// constants - connection related
	static const int DEFAULT_PUBLISHER_PORT = 10001;  // this port will be used for the publisher unless otherwise specified in Godot socket_options
	static const int DEFAULT_LISTENER_PORT = 10002;  // this port will be used for the listener unless otherwise specified in Godot socket_options

	static const std::map<int, int> DEFAULT_PUBLISHER_OPTIONS = {
	{ZMQ_SNDHWM, 10},  // send high watermark (messages dropped when high watermark exceeded)
	{ZMQ_SNDTIMEO, 250},  // send timeout in milliseconds
	};

	static const std::map<int, int> DEFAULT_LISTENER_OPTIONS = {
		{ZMQ_RCVTIMEO, 250}, // receive timeout in milliseconds
		{ZMQ_LINGER, 0}, // pending messages discarded immediately on socket close
		{ZMQ_RCVHWM, 10}, // receive high watermark (messages dropped when high watermark exceeded)
		{ZMQ_REQ_RELAXED, 1}, // relax strict alternation between request and reply
		{ZMQ_REQ_CORRELATE, 1}, // adds extra frame to requests and replies for matching purposes

	};

	// constants - message elements
	static const char* MSG_HEADER = "header";
	static const char* MSG_DATA = "data";

	// constants - header elements
	static const char* SEQNO = "seqno";
	static const char* TIME = "time";

	// shared verbosity variable
	static int verbosity = 0;

	/* Listener Class
	* 
	*  Description: Receives Godot external requests for environment events (e.g., agent actions, or agents joining/leaving the environment).
	*****************************************************************************************************************************************/
	class Listener {
	private:
		zmq::socket_t* p_socket;  // ZeroMq socket backing this connection
		uint16_t port;  // network port number used for socket connection
		uint64_t seqno;  // request sequence numbers

		GodotAiBridge& bridge;  // used to communicate with Godot engine (e.g., sending signals)

		zmq::message_t create_reply(const uint64_t seqno, const std::string& parse_errors);
	public:

		Listener(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port, GodotAiBridge& bridge);

		void operator()();
		void receive(const zmq::message_t& request);
	};

	/* Publisher Class
	*
	*  Description: Broadcasts messages from Godot (e.g., agent state information) to external consumers.
	*****************************************************************************************************************************************/
	class Publisher {
	private:
		zmq::socket_t* p_socket;  // ZeroMq socket backing this connection
		uint16_t port;  // network port number used for socket connection
		uint64_t seqno;  // published message sequence numbers

		void construct_message(zmq::message_t& msg, const std::string& topic, const std::string& payload);
		size_t get_message_length(const std::string& topic, const std::string& msg);

	public:
		Publisher(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port);

		void publish(const std::string& topic, const std::string& content);
		uint64_t get_seqno();
	};

	/* GodotAiBridge Class (subclass of godot::Node)
	*
	*  Description: A Godot Node that functions as the interface between the Godot engine and the communication middleware provided by
	*               this library. It provides the mechanism by which environment state can be sent to Godot external clients, and events
	*               can be requested and sent to Godot from those clients.
	*****************************************************************************************************************************************/
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
		void connect(godot::Variant v_options);  // initializes the network sockets and listener threads. operation can be customized via user supplied options.
		void send(const godot::Variant v_topic, const godot::Variant v_data);  // sends a message from Godot engine to external clients on the specified message topic.
		void notify(const zmq::message_t& request, std::string& parse_errors);  // emits a signal to Godot along with the requested event details
	};

	// Maps socket options from Godot Dictionary to a std::map usable by ZeroMQ
	inline void map_options(const godot::Dictionary& v_options, std::map<int, int>& options_out) {

		// Map from Godot's String options to ZMQ options. The keys in this map are the complete list of connection options available from Godot.
		static std::map<godot::String, int> GODOT_OPTION_TO_ZMQ_MAP = {
			{godot::String("ZMQ_RCVHWM"), ZMQ_RCVHWM},
			{godot::String("ZMQ_RCVTIMEO"), ZMQ_RCVTIMEO},
			{godot::String("ZMQ_SNDHWM"), ZMQ_SNDHWM},
			{godot::String("ZMQ_SNDTIMEO"), ZMQ_SNDTIMEO},
			{godot::String("ZMQ_CONFLATE"), ZMQ_CONFLATE},
		};

		// iterate over Godot dictionary keys
		godot::Array keys = v_options.keys();
		for (int i = 0; i < keys.size(); i++)
		{
			godot::String key = keys[i];

			// if Godot key matches known ZMQ key, then set value in output map
			auto search = GODOT_OPTION_TO_ZMQ_MAP.find(key);
			if (search != GODOT_OPTION_TO_ZMQ_MAP.end()) {
				int zmq_option = search->second;
				int zmq_value = (int)convert_int(v_options[key]);

				options_out[zmq_option] = zmq_value;
			}					
		}
	}

	inline void set_options(zmq::socket_t& socket, std::map<int, int>& socket_options) {
		for (std::map<int, int>::iterator it = socket_options.begin(); it != socket_options.end(); ++it) {
			zmq_setsockopt(socket, it->first, &it->second, sizeof(it->second));
		}
	}

	inline std::string construct_endpoint(int port) {
		return "tcp://*:" + std::to_string(port);
	}

	inline void construct_message_header(json& marshaler, uint64_t seqno)
	{
		using std::chrono::duration_cast;
		using std::chrono::system_clock;
		using std::chrono::milliseconds;		

		marshaler[SEQNO] = seqno;
		marshaler[TIME] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	}

	//inline std::string serialize(const godot::Variant v_payload, uint64_t seqno) {
	//	json marshaler;

	//	json& header = marshaler[MSG_HEADER];
	//	json& data = marshaler[MSG_DATA];

	//	construct_message_header(header, seqno);
	//	marshal_variant(v_payload, data);

	//	return marshaler.dump();
	//}
};