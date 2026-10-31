#pragma once

#include <nlohmann/json.hpp>

#include "library/Models.h"

/// JSON <-> domain model conversions. Field names are camelCase, money is integer cents,
/// timestamps are Unix seconds. Picked up by nlohmann::json via ADL.
namespace library {

void to_json(nlohmann::json& j, const Book& book);
void from_json(const nlohmann::json& j, Book& book);

void to_json(nlohmann::json& j, const Student& student);
void from_json(const nlohmann::json& j, Student& student);

void to_json(nlohmann::json& j, const Loan& loan);
void to_json(nlohmann::json& j, const LibraryStats& stats);

}  // namespace library
