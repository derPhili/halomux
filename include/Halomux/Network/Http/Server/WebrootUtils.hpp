#pragma once

#include <filesystem>

#include <Halomux/EmbeddedWebrootFiles.hpp>
#include <Halomux/Network/Http/Server/HttpDataStructs.hpp>


namespace Halomux::Network::Http::Server::Utils
{
  inline void getWebFile(
    Response& res,
    const std::filesystem::path& filePath
  )
  {
    const static std::unordered_map<std::string, std::string> contentTypes = {
      {".js", "text/javascript"},
      {".html", "text/html"},
      {".css", "text/css"},
      {".svg", "image/svg+xml"}
    };


    auto file = Webroot::embeddedFiles.find(filePath);
    if(file == Halomux::Webroot::embeddedFiles.end()){
      res.contentType = "text/html";
      res.status = 404;
      // 404.html is guaranteed to exist in the embedded webroot.
      res.body = Halomux::Webroot::embeddedFiles.find("404.html")->second;
      return;
    }

    auto contentTypeIt = contentTypes.find(filePath.extension().string());
    if(contentTypeIt != contentTypes.end()){
      res.contentType = contentTypeIt->second;
    }
    else{
      res.contentType = "text/plain";
    }

    res.body = file->second;
  }
}
