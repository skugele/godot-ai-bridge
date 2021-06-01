#pragma once

#include <Godot.hpp>
#include <Node.hpp>
#include <sstream>
#include <zmq.hpp>
#include "Array.hpp"
#include "String.hpp"
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <cerrno>

#include "godot_util.h"

class AgentComm : public godot::Node {
	GODOT_CLASS(AgentComm, Node);

private:

	static const int PORT_MIN;
	static const int PORT_MAX;
	static const int PORT_DEFAULT;

	static const char* AGENT_MSG_HEADER;
	static const char* TOPIC_MSG_HEADER;
	static const char* SEQNO_MSG_HEADER;

	static const char* MSG_HEADER_ELEMENT;
	static const char* MSG_DATA_ELEMENT;

	static const char* PROTOCOL_DEFAULT;

	// Godot constants MUST be initialized at runtime AFTER Godot engine initialization
	const godot::Variant* AGENT_OPTION;
	const godot::Variant* PORT_OPTION;
	const godot::Variant* PROTOCOL_OPTION;

	// zmq context used for all connections
	zmq::context_t* zmq_context;

	typedef unsigned int context_id_t;
	struct ConnectContext {
		context_id_t id;
		std::string agent;
		zmq::socket_t* connection;
		int type;
		int port;
		uint64_t seq_no;
	};

	std::map<context_id_t, ConnectContext*> connection_registry;

	bool receiving_actions = false;

	class ActionListener {
		zmq::socket_t* connection;
		AgentComm* comm;

		const int ZMQ_RCVTIMEO_VALUE = 1000; // timeout in milliseconds
		const int ZMQ_LINGER_VALUE = 0; // pending messages discarded immediately on socket close
		const int ZMQ_RCVHWM_VALUE = 10; // default is 1000

		std::string SUCCESS_REPLY;
		std::string ERROR_REPLY;

	public:
		ActionListener(AgentComm* parent, zmq::context_t* zmq_context, const godot::Dictionary& options) : comm(parent) {
			connection = new zmq::socket_t(*zmq_context, ZMQ_REP);

			zmq_setsockopt(*connection, ZMQ_RCVTIMEO, &ZMQ_RCVTIMEO_VALUE, sizeof(int));
			zmq_setsockopt(*connection, ZMQ_LINGER, &ZMQ_LINGER_VALUE, sizeof(int));
			zmq_setsockopt(*connection, ZMQ_RCVHWM, &ZMQ_RCVHWM_VALUE, sizeof(int));

			std::string endpoint;
			parent->construct_endpoint(options, endpoint);

			connection->bind(endpoint);

			// create fixed content success message
			nlohmann::json marshaler;

			marshaler["status"] = "SUCCESS";
			marshaler["reason"] = "";

			SUCCESS_REPLY = marshaler.dump();

			std::cerr << "starting action listener using socket on " << endpoint << std::endl;
			comm->receiving_actions = true;
		}

		void operator()() {
			std::cerr << "action listener ready to receive action requests" << std::endl;

			while (comm->receiving_actions) {
				zmq::message_t request;

				//  Wait for next request from client
				if (connection->recv(&request)) {
					std::string reply_content = SUCCESS_REPLY;

					// check for parser errors
					std::string parse_errors = "";
					comm->recv_action(request, parse_errors);
					if (! parse_errors.empty()) {
						nlohmann::json marshaler;

						marshaler["status"] = "ERROR";
						marshaler["reason"] = parse_errors;

						reply_content = marshaler.dump();
					}

					//  Send reply back to client
					zmq::message_t reply(reply_content.length());
					memcpy(reply.data(), reply_content.c_str(), reply_content.length());

					connection->send(reply);
				}
			}
			std::cerr << "action listener shutting down" << std::endl;
		}
	};

	//ActionListener* listener;
	std::thread* listener_thread;

	void construct_endpoint(const godot::Dictionary& options, std::string& endpoint);
	void construct_message_header(AgentComm::ConnectContext* context, nlohmann::json& marshaler);
	void construct_message(zmq::message_t& msg, const std::string& topic, const std::string& payload);
	size_t get_message_length(std::string& topic, std::string& msg);
	std::string serialize(const godot::Variant v_payload, ConnectContext* context);
	ConnectContext* lookup_context(const godot::Variant& v_id);
	ConnectContext* register_context(const godot::Dictionary&, int type);
	context_id_t get_unique_id();
	void recv_action(const zmq::message_t& request, std::string& parse_errors);

public:

	AgentComm();
	~AgentComm();

	// GDNative required methods
	static void _register_methods();
	void _init();

	// GDNative exposed methods
	int connect(godot::Variant options);
	void start_listener(godot::Variant v_options);
	void stop_listener();
	void send(const godot::Variant v_content, const godot::Variant v_context, const godot::Variant v_topic);
};