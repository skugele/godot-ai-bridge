#include "util.h"

using json = nlohmann::json;

// this may need to change if Godot's character encoding scheme changes
using convert_type = std::codecvt_utf8<wchar_t>;

std::string gab::convert_string(const godot::String& v) {
	// wstring to string converter
	static std::wstring_convert<convert_type, wchar_t> converter;

	return converter.to_bytes(v.unicode_str());
}

void gab::marshal_basic_variant(const godot::Variant& value, nlohmann::json& marshaler) {

	switch (value.get_type()) {
	case godot::Variant::NIL:
		marshaler = convert_nil(value);
		break;
	case godot::Variant::BOOL:
		marshaler = convert_bool(value);
		break;
	case godot::Variant::INT:
		marshaler = convert_int(value);
		break;
	case godot::Variant::REAL:
		marshaler = convert_real(value);
		break;
	case godot::Variant::STRING:
		marshaler = convert_string(value);
		break;
	default:
		// TODO: Replace with an exception?
		std::cerr << "Unrecognized variant type: " << value.get_type() << std::endl;
		break;
	}
}

void gab::marshal_basic_variant_in_array(const godot::Variant& value, nlohmann::json& marshaler) {

	switch (value.get_type()) {
	case godot::Variant::NIL:
		marshaler.push_back(convert_nil(value));
		break;
	case godot::Variant::BOOL:
		marshaler.push_back(convert_bool(value));
		break;
	case godot::Variant::INT:
		marshaler.push_back(convert_int(value));
		break;
	case godot::Variant::REAL:
		marshaler.push_back(convert_real(value));
		break;
	case godot::Variant::STRING:
		marshaler.push_back(convert_string(value));
		break;
	default:
		GodotAiBridgeException("unrecognized variant type: " + std::to_string(value.get_type()));
	}
}

void gab::marshal_array_variant(const godot::Array& array, nlohmann::json& marshaler) {
	for (int i = 0; i < array.size(); i++) {
		godot::Variant value = array[i];

		if (is_basic_variant(value)) {
			marshal_basic_variant_in_array(value, marshaler);
		}
		else if (is_array_variant(value)) {
			// adds empty array
			marshaler.push_back(json::array());

			// recusrive call
			marshal_array_variant(value, marshaler[marshaler.size() - 1]);
		}
		else if (is_dictionary_variant(value)) {
			// adds empty dictionary
			marshaler.push_back(json({}));
			
			marshal_dictionary_variant(value, marshaler[marshaler.size() - 1]);
		}
	}
}

void gab::marshal_dictionary_variant(const godot::Dictionary& dict, nlohmann::json& marshaler) {

	godot::Array keys = dict.keys();

	for (int i = 0; i < keys.size(); i++)
	{
		godot::Variant key = keys[i];
		godot::Variant value = dict[key];

		json& element = marshaler[convert_string(key)];
		marshal_variant(value, element);
	}
}

void gab::marshal_pool_variant(const godot::Variant& array, nlohmann::json& marshaler)
{
	marshal_array_variant(array, marshaler);
}

void gab::marshal_variant(const godot::Variant& value, nlohmann::json& marshaler) {

	switch (value.get_type()) {
	case godot::Variant::DICTIONARY:
	{
		marshal_dictionary_variant(value, marshaler);
		break;
	}
	case godot::Variant::ARRAY:
	{
		marshal_array_variant(value, marshaler);
		break;
	}
	case godot::Variant::NIL:
	case godot::Variant::BOOL:
	case godot::Variant::INT:
	case godot::Variant::REAL:
	case godot::Variant::STRING:
	{
		marshal_basic_variant(value, marshaler);
		break;
	}
	case godot::Variant::POOL_BYTE_ARRAY:
	case godot::Variant::POOL_INT_ARRAY:
	case godot::Variant::POOL_REAL_ARRAY:
	case godot::Variant::POOL_STRING_ARRAY:
	case godot::Variant::POOL_VECTOR2_ARRAY:
	case godot::Variant::POOL_VECTOR3_ARRAY:
	case godot::Variant::POOL_COLOR_ARRAY:
	{
		marshal_pool_variant(value, marshaler);
		break;
	}
	default:
		throw GodotAiBridgeException("unrecognized variant type: " + std::to_string(value.get_type()));
	}
}

godot::Variant gab::unmarshal_to_variant(nlohmann::json& value) {
	godot::Variant v(0);
	if (value.is_primitive()) {
		v = unmarshal_to_basic_variant(value);
	}
	else if (value.is_structured()) {
		v = unmarshal_to_structured_variant(value);
	}
	else {
		throw GodotAiBridgeException("unmarshal failed (reason: unknown variant type). value = " + std::string(value));
	}
	return v;
}

godot::Variant gab::unmarshal_to_basic_variant(nlohmann::json& value) {
	if (value.is_string()) {
		return unmarshal_to_string_variant(value);
	}
	else if (value.is_null()) {
		return unmarshal_to_nil_variant(value);
	}
	else if (value.is_number_integer()) {
		return unmarshal_to_int_variant(value);
	}
	else if (value.is_number_float()) {
		return unmarshal_to_real_variant(value);
	}
	else if (value.is_boolean()) {
		return unmarshal_to_bool_variant(value);
	}
	else {
		throw GodotAiBridgeException("unmarshal failed (reason: unknown variant type). value = " + std::string(value));
	}
}

godot::Variant gab::unmarshal_to_structured_variant(nlohmann::json& value) {
	godot::Variant v(0);
	if (value.is_array()) {
		v = unmarshal_to_array_variant(value);
	}
	else if (value.is_object()) {
		v = unmarshal_to_dictionary_variant(value);
	}
	else {
		throw GodotAiBridgeException("unmarshal failed (reason: unknown variant type). value = " + std::string(value));
	}
	return v;
}

godot::Variant gab::unmarshal_to_array_variant(nlohmann::json& value) {
	godot::Array array;
	for (json::iterator it = value.begin(); it != value.end(); ++it) {
		array.push_back(unmarshal_to_variant(*it));
	}

	return array;
}

godot::Variant gab::unmarshal_to_dictionary_variant(nlohmann::json& value) {
	godot::Dictionary dict;
	for (auto& kv_pair : value.items()) {
		auto k = kv_pair.key();
		auto v = kv_pair.value();

		godot::Variant v_key(std::string(k).c_str());
		godot::Variant v_value(unmarshal_to_variant(v));

		dict[v_key] = v_value;
	}

	return dict;
}
