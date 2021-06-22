#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gab {
	// top-level message elements
	static const char* MSG_HEADER = "header";
	static const char* MSG_DATA = "data";

	// header elements
	static const char* SEQNO = "seqno";

	// TODO: can this be used for both the publisher's messages and the listener's replies???
	inline void construct_message_header(json& marshaler, uint64_t seq_no)
	{
		marshaler[SEQNO] = seq_no;
	}

	// TODO: this should be moved into the Publisher class as a private method
	inline void construct_message(zmq::message_t& msg, const std::string& topic, const std::string& payload) {
		char* p_buffer = (char*)msg.data();

		// add topic to buffer
		memcpy(p_buffer, topic.c_str(), topic.length());

		// add space
		p_buffer[topic.length()] = ' ';

		// add message payload to buffer
		memcpy(p_buffer + topic.length() + 1, payload.c_str(), payload.length());
	}

	// TODO: this should be moved into the Publisher class as a private method
	inline size_t get_message_length(const std::string& topic, const std::string& msg) {
		return topic.length() + msg.length() + 1; // additional character for space between topic and json
	}

	// TODO: can this be used for both the publisher's messages and the listener's replies???
	inline std::string serialize(const godot::Variant v_payload, uint64_t seq_no) {
		json marshaler;

		json& header = marshaler[MSG_HEADER];
		json& data = marshaler[MSG_DATA];

		construct_message_header(header, seq_no);
		marshal_variant(v_payload, data);

		return marshaler.dump();
	}
};