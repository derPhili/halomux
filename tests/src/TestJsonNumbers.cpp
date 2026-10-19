#include <cstdint>
#include <limits>
#include <stdexcept>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Serialization/Numbers.hpp>

using namespace Halomux;
using Serialization::Json;
using Serialization::readInteger;
using Serialization::readNumber;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }


  template<typename Read>
  bool refused(Read&& read)
  {
    try{
      read();
      return false;
    }
    catch(const std::exception&){
      return true;
    }
  }
}


/**
 * @brief Numbers read from requests and files, which nlohmann would otherwise wrap, cut or cast
 *        out of range
 */
int main()
{
  // Whole numbers
  expect("Whole number", readInteger<unsigned>(Json(128), 1u, 16384u) == 128);
  expect("Lowest", readInteger<int>(Json(0), 0, 3) == 0);
  expect("Highest", readInteger<int>(Json(3), 0, 3) == 3);
  expect("Negative isn't wrapped", refused([](){readInteger<unsigned>(Json(-1), 0u, 16384u);}));
  expect("Beyond the type isn't wrapped", refused([](){readInteger<int>(Json(4294967296), 0, 3);}));
  expect("Above the range", refused([](){readInteger<unsigned>(Json(16385), 1u, 16384u);}));
  expect("Below the range", refused([](){readInteger<unsigned>(Json(0), 1u, 16384u);}));
  expect("Fraction isn't cut", refused([](){readInteger<int>(Json(1.5), 0, 3);}));
  expect("Huge float", refused([](){readInteger<unsigned>(Json(1e10), 0u, std::numeric_limits<unsigned>::max());}));
  expect("Text", refused([](){readInteger<int>(Json("1"), 0, 3);}));
  expect("Largest index", readInteger<size_t>(Json(std::numeric_limits<uint64_t>::max()), size_t{0}, std::numeric_limits<size_t>::max()) == std::numeric_limits<size_t>::max());
  expect("Negative index", refused([](){readInteger<size_t>(Json(-1), size_t{0}, std::numeric_limits<size_t>::max());}));

  // Numbers
  expect("Number", readNumber(Json(0.5), -1.f, 1.f) == 0.5f);
  expect("Whole number as number", readNumber(Json(1), -1.f, 1.f) == 1.f);
  expect("Number above the range", refused([](){readNumber(Json(1.5), -1.f, 1.f);}));
  expect("Number below the range", refused([](){readNumber(Json(-1e30), -1.f, 1.f);}));
  expect("Beyond float isn't infinite", refused([](){readNumber(Json(1e39));}));
  expect("Largest float", readNumber(Json(static_cast<double>(std::numeric_limits<float>::max()))) == std::numeric_limits<float>::max());
  expect("Text as number", refused([](){readNumber(Json("0.5"));}));
  expect("Bool as number", refused([](){readNumber(Json(true));}));

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("JSON numbers: OK");
  return 0;
}
