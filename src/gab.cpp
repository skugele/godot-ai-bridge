#include "gab.h"

using namespace std;
using namespace gab;
using json = nlohmann::json;

/* Implementation of GodotAiBridge Class
 ****************************************/
GodotAiBridge::GodotAiBridge() : zmq_context() {

}

GodotAiBridge::~GodotAiBridge() {
	delete p_listener;
	delete p_publisher;
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

				if (verbosity >= 1) {
					std::cerr << "Godot-AI-Bridge: verbosity set to " << verbosity << std::endl;
				}
			}

			if (option_dict.has(PUBLISHER_PORT)) {
				publisher_port = (int)convert_int(option_dict[PUBLISHER_PORT]);

				if (verbosity >= 1) {
					std::cerr << "Godot-AI-Bridge: setting publisher port to " << publisher_port << std::endl;
				}
			}

			if (option_dict.has(LISTENER_PORT)) {
				listener_port = (int)convert_int(option_dict[LISTENER_PORT]);

				if (verbosity >= 1) {
					std::cerr << "Godot-AI-Bridge: setting listener port to " << listener_port << std::endl;
				}
			}

			if (option_dict.has(SOCKET_OPTIONS)) {
				godot::Variant socket_opts = option_dict[SOCKET_OPTIONS];

				if (is_dictionary_variant(socket_opts)) {
					map_options(socket_opts, publisher_options);
					map_options(socket_opts, listener_options);
				}

				if (verbosity >= 1) {
					std::cerr << "Godot-AI-Bridge: using custom socket options" << std::endl;
				}
			}

			// TODO: display the resulting socket options for the publisher and listener if verbosity >= 1
		}
			
		p_publisher = new Publisher(zmq_context, publisher_options, publisher_port);
		p_listener = new Listener(zmq_context, listener_options, listener_port, *this);
		
		// start event listener thread
		p_listener_thread = new thread(*p_listener);
	}
	catch (exception& e)
	{
		std::cerr << "Godot-AI-Bridge: encountered exception during call to \"connect\" -> " << e.what() << std::endl;
	}
}

// emit signal to Godot with event details
void GodotAiBridge::notify(const zmq::message_t& request, std::string& parse_errors) {
	char* buffer = new char[request.size() + 1];
	memcpy(buffer, request.data(), request.size());
	buffer[request.size()] = '\0';

	if (verbosity >= 2) {
		std::cout << "Godot-AI-Bridge: emitting \"event_requested\" signal to Godot" << std::endl;
	}

	try {
		auto j = json::parse(buffer);
		godot::Variant v = unmarshal_to_variant(j);

		emit_signal("event_requested", v);
	}
	catch (const json::parse_error& e) {
		std::cerr << "Godot-AI-Bridge: errors occurred when parsing request -> " << e.what() << std::endl;
		parse_errors = e.what();
	}

	delete[] buffer;
}

void GodotAiBridge::send(const godot::Variant v_topic, const godot::Variant v_content)
{
	// Convert godot data types to std::strings
	std::string topic = convert_string(v_topic);
	std::string content = serialize(v_content, p_publisher->get_seq_no());

	p_publisher->publish(topic, content);
}


/* Implementation of Listener Class
 ***********************************/
Listener::Listener(zmq::context_t& zmq_context, std::map<int, int> socket_options, uint16_t port, GodotAiBridge& bridge)
	: seq_no(1),
	  bridge(bridge)
{
	// initialize socket
	p_socket = new zmq::socket_t(zmq_context, ZMQ_REP);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	p_socket->bind(endpoint);
	std::cerr << "Godot-AI-Bridge: listener connected to " << endpoint << std::endl;
}

void Listener::operator()()
{
	std::cerr << "Godot-AI-Bridge: listener receiving requests" << std::endl;

	for (;;) {
		zmq::message_t request;

		//  wait for next request from client
		if (p_socket->recv(&request)) {
			receive(request);
		}
	}
}

void Listener::receive(const zmq::message_t& request)
{
	if (verbosity >= 2) {
		std::cerr << "Godot-AI-Bridge: listener received request (seqno: " << seq_no << ") " << std::endl;
	}

	if (verbosity >= 3) {
		std::cerr << "Godot-AI-Bridge: request contents -> " << (char*)request.data() << std::endl;
	}

	// TODO: split parsing and signal generation
	std::string parse_errors = "";
	bridge.notify(request, parse_errors);

	zmq::message_t reply = create_reply(seq_no, parse_errors);
	p_socket->send(reply);
	seq_no++;
}

zmq::message_t Listener::create_reply(const uint64_t seq_no, const std::string& parse_errors)
{
	nlohmann::json marshaler;
	json& header = marshaler[MSG_HEADER];
	json& data = marshaler[MSG_DATA];

	header[SEQNO] = seq_no;

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
	: seq_no(1)
{
	// initialize socket
	p_socket = new zmq::socket_t(zmq_context, ZMQ_PUB);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	p_socket->bind(endpoint);
	std::cerr << "Godot-AI-Bridge: publisher connected to " << endpoint << std::endl;
}

void Publisher::publish(const std::string& topic, const std::string& content)
{
	try
	{
		zmq::message_t message(get_message_length(topic, content));
		construct_message(message, topic, content);

		if (verbosity >= 2) {
			std::cerr << "Godot-AI-Bridge: publishing message (seqno: " << seq_no << ", topic: " << topic << ") " << std::endl;
		}

		if (verbosity >= 3) {
			std::cerr << "Godot-AI-Bridge: message contents -> " << content << std::endl;
		}

		p_socket->send(message);
		seq_no++;
	}
	catch (exception& e)
	{
		std::cout << "Godot-AI-Bridge: encountered exception when publishing message -> " << e.what() << std::endl;
	}
}

uint64_t Publisher::get_seq_no()
{
	return seq_no;
}
