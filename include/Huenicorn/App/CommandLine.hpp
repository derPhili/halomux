#pragma once

#include <string>
#include <string_view>
#include <vector>


namespace Huenicorn::App::CommandLine
{
  struct Params
  {
    bool shouldStop{false};
  };

  using ParamAction = void(*)(Params&);

  struct Option
  {
    std::vector<std::string_view> names;
    ParamAction action;
    std::string description;
  };


  using Options = std::vector<Option>;


  void getOpt(
    int argc,
    char* argv[],
    Params& params
  );
}
