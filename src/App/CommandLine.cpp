#include <sstream>
#include <Huenicorn/App/CommandLine.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Version.hpp>


namespace Huenicorn::App::CommandLine
{
  void showHelp(
    Params& params
  );

  void showVersion(
    Params& params
  );


  const Options options = {
    {
      {"-h", "--help"},
      showHelp,
      "Displays this help",
    },
    {
      {"-v", "--version"},
      showVersion,
      "Displays current huenicorn version",
    }
  };


  void showHelp(
    Params& params
  )
  {
    Huenicorn::Core::Logger::log("Usage:");

    for(const auto& option : options){
      std::stringstream names;
      for(size_t i = 0; i < option.names.size(); i++){
        names << option.names[i];
        if(i + 1 < option.names.size()){
          names << ", ";
        }
      }
      Huenicorn::Core::Logger::log("\t", names.str(), "\t", option.description);
    }

    params.shouldStop = true;
  }


  void showVersion(
    Params& params
  )
  {
    Huenicorn::Core::Logger::log("Huenicorn ", Huenicorn::Version);
    params.shouldStop = true;
  }

  inline const Option* findOption(
    std::string_view arg
  )
  {
    for(const auto& option : options){
      if(std::ranges::find(option.names, arg) != option.names.end()){
        return &option;
      }
    }

    return nullptr;
  }


  void getOpt(
    int argc,
    char* argv[],
    Params& params
  )
  {
    for(int i = 1; i < argc && !params.shouldStop; i++){
      auto* option = findOption(argv[i]);
      if(!option){
        Huenicorn::Core::Logger::log("huenicorn: Invalid parameter '", argv[i], "'");
        showHelp(params);
        return;
      }
      option->action(params);
    }
  }
}
