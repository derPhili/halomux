#pragma once


namespace Huenicorn::Core
{
  /**
   * @brief Version of the notice the user accepts before using the application
   *
   * The notice states that the application is developed with AI assistance and comes without
   * warranty or liability (webroot/Notice.js). Increase this number whenever its text changes:
   * users who accepted an older version are asked to accept the new one.
   */
  inline constexpr unsigned NoticeVersion = 2;
}
