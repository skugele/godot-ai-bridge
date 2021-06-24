#include "gab.h"

using namespace std;
using namespace gab;

/* Implementation of GodotAiBridge Class
 ****************************************/
GodotAiBridge::GodotAiBridge() : zmq_context() {

}

GodotAiBridge::~GodotAiBridge() {
	if (p_listener != nullptr)
		delete p_listener;

	if (p_publisher != nullptr)
		delete p_publisher;

	if (p_listener_thread != nullptr)
		delete p_listener_thread;
}


void GodotAiBridge::_register_methods() {
	godot::register_method("connect", &GodotAiBridge::connect);
	godot::register_method("send", &GodotAiBridge::send);
	
	godot::register_signal<gab::GodotAiBridge>("event_requested", "event_details", GODOT_VARIANT_TYPE_DICTIONARY);
}

void GodotAiBridge::_init() {
	cout << "Godot-AI-Bridge: initializing..." << endl;
}

void GodotAiBridge::connect(godot::Variant v_options) {
	try
	{
		int publisher_port = DEFAULT_PUBLISHER_PORT;
		int listener_port = DEFAULT_LISTENER_PORT;

		std::map<int, int> publisher_options(DEFAULT_PUBLISHER_OPTIONS);
		std::map<int, int> listener_options(DEFAULT_LISTENER_OPTIONS);

		if (is_dictionary_variant(v_options)) {
			godot::Dictionary option_dict(v_options);

			static const godot::String PUBLISHER_PORT = "publisher_port";
			static const godot::String LISTENER_PORT = "listener_port";
			static const godot::String SOCKET_OPTIONS = "socket_options";
			static const godot::String VERBOSITY = "verbosity";

			if (option_dict.has(VERBOSITY)) {
				verbosity = (int)convert_int(option_dict[VERBOSITY]);

				if (verbosity >= DEBUG) {
					std::cerr << "Godot-AI-Bridge: verbosity set to " << verbosity << std::endl;
				}
			}

			if (option_dict.has(PUBLISHER_PORT)) {
				publisher_port = (int)convert_int(option_dict[PUBLISHER_PORT]);

				if (verbosity >= DEBUG) {
					std::cerr << "Godot-AI-Bridge: setting publisher port to " << publisher_port << std::endl;
				}
			}

			if (option_dict.has(LISTENER_PORT)) {
				listener_port = (int)convert_int(option_dict[LISTENER_PORT]);

				if (verbosity >= DEBUG) {
					std::cerr << "Godot-AI-Bridge: setting listener port to " << listener_port << std::endl;
				}
			}

			if (option_dict.has(SOCKET_OPTIONS)) {
				godot::Variant socket_opts = option_dict[SOCKET_OPTIONS];

				if (is_dictionary_variant(socket_opts)) {
					map_options(socket_opts, publisher_options);
					map_options(socket_opts, listener_options);
				}

				if (verbosity >= DEBUG) {
					std::cerr << "Godot-AI-Bridge: using custom socket options" << std::endl;
				}
			}
		}
			
		p_publisher = new Publisher(zmq_context, publisher_options, publisher_port);
		p_listener = new Listener(zmq_context, listener_options, listener_port, *this);
		
		// start event listener thread
		p_listener_thread = new thread(*p_listener);
	}
	catch (exception& e)
	{
		std::cerr << "Godot-AI-Bridge: encountered fatal exception during call to \"connect\" -> " << e.what() << std::endl;
	}
}

// emit signal to Godot with event details
void GodotAiBridge::notify(const zmq::message_t& request, std::string& parse_errors) {
	char* buffer = new char[request.size() + 1];
	memcpy(buffer, request.data(), request.size());
	buffer[request.size()] = '\0';

	if (verbosity >= DEBUG) {
		std::cout << "Godot-AI-Bridge: emitting \"event_requested\" signal to Godot" << std::endl;
	}

	try {
		auto j = json::parse(buffer);
		godot::Variant v = unmarshal_to_variant(j);

		emit_signal("event_requested", v);
	}
	catch (const json::parse_error& e) {
		if (verbosity >= ERROR) {
			std::cerr << "Godot-AI-Bridge: errors occurred when parsing request -> " << e.what() << std::endl;
		}
		parse_errors = e.what();
	}

	delete[] buffer;
}

void GodotAiBridge::send(const godot::Variant v_topic, const godot::Variant v_data)
{
	json marshaler;
	json& header = marshaler[MSG_HEADER];
	json& data = marshaler[MSG_DATA];

	construct_message_header(header, p_publisher->get_seqno());
	marshal_variant(v_data, data);

	std::string topic = convert_string(v_topic);
	std::string content = marshaler.dump();

	p_publisher->publish(topic, content);
}


/* Implementation of Listener Class
 ***********************************/
Listener::Listener(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port, GodotAiBridge& bridge)
	: seqno(1),
	  bridge(bridge)
{
	// initialize socket
	p_socket = new zmq::socket_t(zmq_context, ZMQ_REP);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	p_socket->bind(endpoint);

	if (verbosity >= INFO) {
		std::cerr << "Godot-AI-Bridge: listener connected to " << endpoint << std::endl;
	}
}

void Listener::operator()()
{
	if (verbosity >= INFO) {
		std::cerr << "Godot-AI-Bridge: listener receiving requests" << std::endl;
	}

	for (;;) {
		zmq::message_t request;

		// wait for next request from client
		if (p_socket->recv(request, zmq::recv_flags::none)) {
			receive(request);
		}
	}
}

void Listener::receive(const zmq::message_t& request)
{
	if (verbosity >= DEBUG) {
		std::cerr << "Godot-AI-Bridge: listener received request (seqno: " << seqno << ") " << std::endl;
	}

	if (verbosity >= TRACE) {
		std::cerr << "Godot-AI-Bridge: request contents -> " << (char*)request.data() << std::endl;
	}

	std::string parse_errors = "";
	bridge.notify(request, parse_errors);

	zmq::message_t reply = create_reply(seqno, parse_errors);

	if (verbosity >= DEBUG) {
		std::cerr << "Godot-AI-Bridge: listener sending reply (seqno: " << seqno << ") " << std::endl;
	}

	if (verbosity >= TRACE) {
		std::cerr << "Godot-AI-Bridge: reply contents -> " << (char*)reply.data() << std::endl;
	}

	p_socket->send(reply, zmq::send_flags::none);

	seqno++;
}

zmq::message_t Listener::create_reply(const uint64_t seqno, const std::string& parse_errors)
{
	json marshaler;
	json& header = marshaler[MSG_HEADER];
	json& data = marshaler[MSG_DATA];

	construct_message_header(header, seqno);

	// SUCCESS reply
	if (parse_errors.empty())
	{
		data["status"] = "SUCCESS";
	}

	// ERROR reply
	else
	{
		data["status"] = "ERROR";
		data["reason"] = parse_errors;
	}

	std::string reply_content = marshaler.dump();

	zmq::message_t reply(reply_content.length());
	memcpy(reply.data(), reply_content.c_str(), reply_content.length());

	return reply;
}


/* Implementation of Publisher Class 
 ************************************/
Publisher::Publisher(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port)
	: seqno(1)
{
	// initialize socket
	p_socket = new zmq::socket_t(zmq_context, ZMQ_PUB);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	p_socket->bind(endpoint);

	if (verbosity >= INFO) {
		std::cerr << "Godot-AI-Bridge: publisher connected to " << endpoint << std::endl;
	}
}

void Publisher::publish(const std::string& topic, const std::string& content)
{
	try
	{
		zmq::message_t message(get_message_length(topic, content));
		construct_message(message, topic, content);

		if (verbosity >= DEBUG) {
			std::cerr << "Godot-AI-Bridge: publishing message (seqno: " << seqno << ", topic: " << topic << ") " << std::endl;
		}

		if (verbosity >= TRACE) {
			std::cerr << "Godot-AI-Bridge: message contents -> " << content << std::endl;
		}

		p_socket->send(message, zmq::send_flags::none);
		seqno++;
	}
	catch (exception& e)
	{
		if (verbosity >= ERROR) {
			std::cout << "Godot-AI-Bridge: encountered exception when publishing message -> " << e.what() << std::endl;
		}
	}
}

void Publisher::construct_message(zmq::message_t& msg, const std::string& topic, const std::string& payload) 
{
	char* p_buffer = (char*)msg.data();

	// add topic to buffer
	memcpy(p_buffer, topic.c_str(), topic.length());

	// add space
	p_buffer[topic.length()] = ' ';

	// add message payload to buffer
	memcpy(p_buffer + topic.length() + 1, payload.c_str(), payload.length());
}

size_t Publisher::get_message_length(const std::string& topic, const std::string& msg) 
{
	return topic.length() + msg.length() + 1; // additional character for space between topic and json
}

uint64_t Publisher::get_seqno()
{
	return seqno;
}

// Maps socket options from Godot Dictionary to a std::map usable by ZeroMQ
void gab::map_options(const godot::Dictionary& v_options, std::map<int, int>& options_out)
{
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
