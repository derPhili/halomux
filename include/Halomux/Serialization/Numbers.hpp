#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Serialization
{
  /**
   * @brief Reads a whole number from a request or a file
   *
   * nlohmann's get<>() converts any number: it wraps negative and too large ones and cuts
   * fractions, so -1 read as unsigned is about four billion.
   *
   * @param value JSON value
   * @param min Lowest accepted number
   * @param max Highest accepted number
   * @return Integer The number
   * @throw std::invalid_argument if it isn't a whole number from min to max
   */
  template<typename Integer>
  Integer readInteger(
    const Json& value,
    Integer min,
    Integer max
  )
  {
    if(!value.is_number_integer()){
      throw std::invalid_argument("not a whole number");
    }

    const bool inRange = value.is_number_unsigned()
      ? std::cmp_greater_equal(value.get<uint64_t>(), min) && std::cmp_less_equal(value.get<uint64_t>(), max)
      : std::cmp_greater_equal(value.get<int64_t>(), min) && std::cmp_less_equal(value.get<int64_t>(), max);
    if(!inRange){
      throw std::invalid_argument("number out of range");
    }

    return value.is_number_unsigned() ? static_cast<Integer>(value.get<uint64_t>()) : static_cast<Integer>(value.get<int64_t>());
  }


  /**
   * @brief Reads a number from a request or a file, as a float
   *
   * A number beyond float would become infinite, which no setting expects.
   *
   * @param value JSON value
   * @param min Lowest accepted number
   * @param max Highest accepted number
   * @return float The number
   * @throw std::invalid_argument if it isn't a number from min to max
   */
  inline float readNumber(
    const Json& value,
    float min = std::numeric_limits<float>::lowest(),
    float max = std::numeric_limits<float>::max()
  )
  {
    if(!value.is_number()){
      throw std::invalid_argument("not a number");
    }

    const double number = value.get<double>();
    if(!(number >= min && number <= max)){
      throw std::invalid_argument("number out of range");
    }

    return static_cast<float>(number);
  }
}
