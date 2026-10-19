#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>

#include <httplib.h>

#include <nlohmann/json.hpp>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Server/HostCheck.hpp>
#include <Halomux/Network/Http/Server/RouteTools.hpp>
#include <Halomux/Network/Http/Server/HttpDataStructs.hpp>


namespace Halomux::Network::Http::Server
{
  class HttpServer;


  class Impl
  {
    friend HttpServer;

    /**
     * @brief Checks that a browser request originates from the web UI itself
     *
     * @param req Incoming request
     * @return true The request carries no Origin header (non-browser client) or it matches the Host header
     * @return false The request was issued by a page from another origin
     */
    static bool _isSameOrigin(const httplib::Request& req)
    {
      if(!req.has_header("Origin")){
        return true;
      }

      const std::string origin = req.get_header_value("Origin");
      const std::string host = req.get_header_value("Host");

      return !host.empty() && (origin == "http://" + host || origin == "https://" + host);
    }


    static Response _errorResponse(int status, const std::string& message)
    {
      return {
        .status = status,
        .contentType = "application/json",
        .body = nlohmann::json{{"succeeded", false}, {"error", message}}.dump()
      };
    }


    static httplib::Server::Handler _wrapHandler(Handler handler, HttpMethod method)
    {
      return [handler = std::move(handler), method](const httplib::Request& req, httplib::Response& res){
        if(!isAllowedHost(req.get_header_value("Host"))){
          // A page of another site whose name resolves to this machine (DNS rebinding)
          res.status = 403;
          res.set_content("{\"succeeded\": false, \"error\": \"unknown host\"}", "application/json");
          return;
        }

        if(method != HttpMethod::Get && !_isSameOrigin(req)){
          // Prevents other web pages open in the user's browser from driving the API (CSRF)
          res.status = 403;
          res.set_content("{\"succeeded\": false, \"error\": \"cross-origin request rejected\"}", "application/json");
          return;
        }

        Request r;
        Response w;

        r.method = method;
        r.body = req.body;
        r.path = req.path;

        for(const auto& [key, value] : req.path_params){
          r.pathParams[key] = value;
        }

        for(const auto& [key, value] : req.params){
          r.queryParams[key] = value;
        }

        try{
          handler(r, w);
        }
        catch(const HttpError& e){
          w = _errorResponse(e.status(), e.what());
        }
        catch(const std::exception& e){
          // A bug rather than a bad request: logged, not disclosed
          Core::Logger::error("Request ", req.method, " ", req.path, " failed: ", e.what());
          w = _errorResponse(500, "internal error");
        }

        res.status = w.status;

        // Brotli costs more than it saves on small responses: with the web UI polling the
        // lamp colors several times per second, it took most of the process' CPU time.
        // httplib doesn't compress responses with a fixed length content provider.
        static constexpr size_t CompressionThreshold = 1024;
        if(!w.body.empty() && w.body.size() < CompressionThreshold){
          auto body = std::make_shared<const std::string>(std::move(w.body));
          res.set_content_provider(body->size(), w.contentType, [body](size_t offset, size_t length, httplib::DataSink& sink){
            return sink.write(body->data() + offset, length);
          });
        }
        else{
          res.set_content(w.body, w.contentType);
        }
      };
    }

  public:
    ~Impl()
    {
      stop();
    }


    Impl(const std::vector<Route>& routes)
    {
      m_service.emplace();

      auto options = httplib::SocketOptions{};

      m_service->set_socket_options([](socket_t sock) {
        httplib::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
      });

      // On every answer, refused and unknown ones too: another site framing the light manager
      // could have its buttons clicked unseen (clickjacking), past the Origin check
      m_service->set_default_headers({
        {"X-Frame-Options", "DENY"},
        {"Content-Security-Policy", "frame-ancestors 'none'"},
        {"X-Content-Type-Options", "nosniff"}
      });

      // The requests carry small JSON documents: anything larger is refused before it's read
      static constexpr size_t MaxRequestBodySize = 64 * 1024;
      m_service->set_payload_max_length(MaxRequestBodySize);

      for(const auto& route : routes){
        auto wrapped = _wrapHandler(route.handler, route.method);
        switch(route.method)
        {
          case HttpMethod::Get:
            m_service->Get(route.path, wrapped);
            break;

          case HttpMethod::Post:
            m_service->Post(route.path, wrapped);
            break;

          case HttpMethod::Put:
            m_service->Put(route.path, wrapped);
            break;

          case HttpMethod::Delete:
            m_service->Delete(route.path, wrapped);
            break;

          case HttpMethod::Patch:
            m_service->Patch(route.path, wrapped);
            break;
        }
      }
    }


    bool stop()
    {
      if(!m_service.has_value()){
        return false;
      }

      // httplib asserts when stopped again while still shutting down
      if(m_stopRequested.exchange(true)){
        return true;
      }

      // httplib ignores a stop until it runs: a listen() that passed its check but doesn't run
      // yet is waited for, briefly. From a handler it already runs, so this never waits there.
      while(m_listening && !m_service->is_running()){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      // Only signal the server to stop. The httplib::Server instance must outlive
      // the thread blocked in listen() and the worker currently running a handler
      // (e.g. /api/finishSetup stops the server from inside its own handler).
      // It is destroyed along with this Impl.
      m_service->stop();

      return true;
    }


    bool bind(
      const std::string& boundAddress,
      unsigned port
    )
    {
      return m_service->bind_to_port(boundAddress, port);
    }


    bool listen()
    {
      if(!m_service){
        return false;
      }

      // Set before the stop is checked: a stop either is seen here, or sees this and waits
      m_listening = true;
      if(m_stopRequested){
        m_listening = false;
        return false;
      }

      const bool listened = m_service->listen_after_bind();
      m_listening = false;
      return listened;
    }

    std::optional<httplib::Server> m_service;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_listening{false};
  };
}
