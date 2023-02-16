#pragma once
#include <toml.hpp>

namespace simple {

using toml_value_t = toml::basic_value<TOML11_DEFAULT_COMMENT_STRATEGY>;
using toml_table_t = toml_value_t::table_type;
using toml_array_t = toml_value_t::array_type;
using toml_key_t = toml_value_t::key_type;
using toml_boolean_t = toml_value_t::boolean_type;
using toml_integer_t = toml_value_t::integer_type;
using toml_floating_t = toml_value_t::floating_type;
using toml_string_t = toml_value_t::string_type;
using toml_local_time_t = toml_value_t::local_time_type;
using toml_local_date_t = toml_value_t::local_date_type;
using toml_local_datetime_t = toml_value_t::local_datetime_type;
using toml_offset_datetime_t = toml_value_t::offset_datetime_type;

}  // namespace simple
