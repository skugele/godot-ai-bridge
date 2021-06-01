#pragma once

#include <Godot.hpp>
#include <string.h>
#include <nlohmann/json.hpp>

inline std::string convert_string(const godot::Variant& v) {
	return godot::String(v).utf8().get_data();
}

inline int64_t convert_int(const godot::Variant& v) {
	return int64_t(v);
}

inline double convert_real(const godot::Variant& v) {
	return double(v);
}

inline bool convert_bool(const godot::Variant& v) {
	return bool(v);
}

inline std::nullptr_t convert_nil(const godot::Variant& v) {
	return nullptr;
}

inline bool is_basic_variant(const godot::Variant& v) {
	switch (v.get_type()) {
	case godot::Variant::NIL:
	case godot::Variant::BOOL:
	case godot::Variant::INT:
	case godot::Variant::REAL:
	case godot::Variant::STRING:
		return true;
		break;
	default:
		return false;
	}
}

inline bool is_array_variant(const godot::Variant& v) {
	return v.get_type() == godot::Variant::ARRAY;
}

inline bool is_dictionary_variant(const godot::Variant& v) {
	return v.get_type() == godot::Variant::DICTIONARY;
}

void marshal_basic_variant(const godot::Variant& value, nlohmann::json& marshaler);
void marshal_basic_variant_in_array(const godot::Variant& value, nlohmann::json& marshaler);

void marshal_array_variant(const godot::Array& dict, nlohmann::json& marshaler);
void marshal_dictionary_variant(const godot::Dictionary& dict, nlohmann::json& marshaler);

void marshal_variant(const godot::Variant& value, nlohmann::json& marshaler);

godot::Variant unmarshal_to_basic_variant(nlohmann::json& value);
godot::Variant unmarshal_to_structured_variant(nlohmann::json& value);

inline godot::Variant unmarshal_to_string_variant(nlohmann::json& value) {
	return godot::Variant(std::string(value).c_str());
}

inline godot::Variant unmarshal_to_int_variant(nlohmann::json& value) {
	return godot::Variant(uint64_t(value));
}

inline godot::Variant unmarshal_to_real_variant(nlohmann::json& value) {
	return godot::Variant(double(value));
}

inline godot::Variant unmarshal_to_bool_variant(nlohmann::json& value) {
	return godot::Variant(bool(value));
}

inline godot::Variant unmarshal_to_nil_variant(nlohmann::json& value) {
	return godot::Variant(NULL);
}

godot::Variant unmarshal_to_variant(nlohmann::json& value);
godot::Variant unmarshal_to_array_variant(nlohmann::json& value);
godot::Variant unmarshal_to_dictionary_variant(nlohmann::json& value);