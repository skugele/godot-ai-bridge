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
	// Register methods
	godot::register_method("connect", &GodotAiBridge::connect);
	godot::register_method("send", &GodotAiBridge::send);
	
	godot::register_signal<gab::GodotAiBridge>("event_requested", "event_details", GODOT_VARIANT_TYPE_DICTIONARY);
}

void GodotAiBridge::_init() {
	cout << "Initializing Godot-AI-Bridge" << endl;
}

void GodotAiBridge::connect(godot::Variant v_options) {

	if (v_options != nullptr && ! is_dictionary_variant(v_options)) {
		std::cerr << "Invalid connect options. Argument must be a dictionary." << std::endl;
		return;
	}

	try
	{
		/* initialize publisher connection
		 *********************************/

		// TODO: PROCESS OPTIONS (e.g., PORT ASSIGNMENTS)
		// TODO: Need to merge default options with user requested options
		std::map<int, int> publisher_options = { {ZMQ_SNDHWM, 10} };
		p_publisher = new Publisher(zmq_context, publisher_options, DEFAULT_PUBLISHER_PORT);


		/* initialize event listener connection
		 **************************************/

		// TODO: PROCESS OPTIONS (e.g., PORT ASSIGNMENTS)
		// TODO: Need to merge default options with user requested options
		std::map<int, int> listener_options = { 
			{ZMQ_RCVTIMEO, 1000}, // receive timeout in milliseconds
			{ZMQ_LINGER, 0}, // pending messages discarded immediately on socket close
			{ZMQ_RCVHWM, 10} // receive high watermark (messages dropped when high watermark exceeded)
		};

		p_listener = new Listener(zmq_context, listener_options, DEFAULT_LISTENER_PORT, *this);
		
		// start event listener thread
		p_listener_thread = new thread(*p_listener);
	}
	catch (exception& e)
	{
		std::cerr << "caught exception: " << e.what() << std::endl;
	}
}

// TODO: split parsing and signal generation
// emit signal to Godot with event details
void GodotAiBridge::notify(const zmq::message_t& request, std::string& parse_errors) {
	char* buffer = new char[request.size() + 1];
	memcpy(buffer, request.data(), request.size());
	buffer[request.size()] = '\0';

	// TODO: wrap this in a "verbose" flag
	std::cout << "received event request: " << buffer << std::endl;

	try {
		auto j = json::parse(buffer);
		godot::Variant v = unmarshal_to_variant(j);

		emit_signal("event_requested", v);
	}
	catch (const json::parse_error& e) {
		std::cerr << "Received parse error when parsing incoming message: " << e.what() << std::endl;
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
	: seq_no(0),
	  bridge(bridge)
{
	// initialize socket
	// TODO: rename this to p_socket...
	p_socket = new zmq::socket_t(zmq_context, ZMQ_REP);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	std::cerr << "opening connection to " << endpoint << "..." << std::endl;
	p_socket->bind(endpoint);
	std::cerr << "connection established!" << std::endl;
}

void Listener::operator()()
{
	std::cerr << "Godot-AI-Bridge event listener ready to receive requests" << std::endl;

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
	// TODO: split parsing and signal generation
	std::string parse_errors = "";
	bridge.notify(request, parse_errors);

	zmq::message_t reply = create_reply(++seq_no, parse_errors);
	p_socket->send(reply);
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
	: seq_no(0)
{
	// initialize socket
	p_socket = new zmq::socket_t(zmq_context, ZMQ_PUB);

	// set socket options
	set_options(*p_socket, socket_options);

	// bind socket connection
	std::string endpoint = "tcp://*:" + std::to_string(port);
	std::cerr << "message publisher opening connection on " << endpoint << "..." << std::endl;
	p_socket->bind(endpoint);
	std::cerr << "connection established!" << std::endl;
}

void Publisher::publish(const std::string& topic, const std::string& content)
{
	try
	{
		zmq::message_t message(get_message_length(topic, content));
		construct_message(message, topic, content);

		p_socket->send(message);
		seq_no++;
	}
	catch (exception& e)
	{
		std::cout << "caught exception: " << e.what() << std::endl;
	}
}

uint64_t Publisher::get_seq_no()
{
	return seq_no;
}
