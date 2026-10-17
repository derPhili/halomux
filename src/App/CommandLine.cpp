#include <sstream>
#include <Halomux/App/CommandLine.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Version.hpp>


namespace Halomux::App::CommandLine
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
      "Displays the current version",
    }
  };


  void showHelp(
    Params& params
  )
  {
    Halomux::Core::Logger::log("Usage:");

    for(const auto& option : options){
      std::stringstream names;
      for(size_t i = 0; i < option.names.size(); i++){
        names << option.names[i];
        if(i + 1 < option.names.size()){
          names << ", ";
        }
      }
      Halomux::Core::Logger::log("\t", names.str(), "\t", option.description);
    }

    params.shouldStop = true;
  }


  void showVersion(
    Params& params
  )
  {
    Halomux::Core::Logger::log(Halomux::AppName, " ", Halomux::Version);
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
        Halomux::Core::Logger::log(Halomux::AppId, ": Invalid parameter '", argv[i], "'");
        showHelp(params);
        return;
      }
      option->action(params);
    }
  }
}
