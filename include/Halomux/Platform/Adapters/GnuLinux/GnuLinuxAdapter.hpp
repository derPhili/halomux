#pragma once

#include <Halomux/Platform/IAdapter.hpp>


namespace Halomux::Platform
{
  class GnuLinuxAdapter : public IAdapter
  {
  public:
    GnuLinuxAdapter():IAdapter{"Gnu/Linux"}{}

    virtual std::filesystem::path getConfigFilePath() const override;

    virtual std::string getUsername() const override;

    virtual void openWebBrowser(
      const std::string& url
    ) const override;

    virtual std::string serviceJournal(
      size_t lines
    ) const override;

  protected:
    virtual SharedGrabber _createGrabber(
      Core::Config* config,
      bool askForMonitors,
      std::stop_token stopToken = {}
    ) override;
  };
}
