#include <fstream>
#include <sstream>
#include <iostream>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

namespace dxvk {

  const static std::unordered_map<std::string, Config> g_appDefaults = {{
    /* Anno 1800                                  */
    { "Anno1800.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    /* Assassin's Creed Syndicate: amdags issues  */
    { "ACS.exe", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Dishonored 2                               */
    { "Dishonored2.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    /* Dissidia Final Fantasy NT Free Edition */
    { "dffnt.exe", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Elite Dangerous: Compiles weird shaders    *
     * when running on AMD hardware               */
    { "EliteDangerous64.exe", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Vanishing of Ethan Carter Redux        */
    { "EthanCarter-Win64-Shipping.exe", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Evil Within: Submits command lists     * 
     * multiple times                             */
    { "EvilWithin.exe", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* The Evil Within Demo                       */
    { "EvilWithinDemo.exe", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Far Cry 3: Assumes clear(0.5) on an UNORM  *
     * format to result in 128 on AMD and 127 on  *
     * Nvidia. We assume that the Vulkan drivers  *
     * match the clear behaviour of D3D11.        */
    { "farcry3_d3d11.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    { "fc3_blooddragon_d3d11.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry 4: Same as Far Cry 3               */
    { "FarCry4.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry 5: Avoid CPU <-> GPU sync          */
    { "FarCry5.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    /* Far Cry Primal: Nvidia performance         */
    { "FCPrimal.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    } }},
    /* Frostpunk: Renders one frame with D3D9     *
     * after creating the DXGI swap chain         */
    { "Frostpunk.exe", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Nioh: See Frostpunk, apparently?           */
    { "nioh.exe", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Quantum Break: Mever initializes shared    *
     * memory in one of its compute shaders       */
    { "QuantumBreak.exe", {{
      { "d3d11.zeroInitWorkgroupMemory",    "True" },
    }} },
    /* Anno 2205: Random crashes with state cache */
    { "anno2205.exe", {{
      { "dxvk.enableStateCache",            "False" },
    }} },
    /* Fifa '19: Binds typed buffer SRV to shader *
     * that expects raw/structured buffer SRV     */
    { "FIFA19.exe", {{
      { "dxvk.useRawSsbo",                  "True" },
    }} },
    /* Final Fantasy XIV: Fix random black blocks */
    { "ffxiv_dx11.exe", {{
      { "d3d11.strictDivision",             "True" },
    }} },
    /* Fifa '19 Demo                              */
    { "FIFA19_demo.exe", {{
      { "dxvk.useRawSsbo",                  "True" },
    }} },
    /* Resident Evil 2: Improve GPU performance   */
    { "re2.exe", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Resident Evil 7                            */
    { "re7.exe", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Devil May Cry 5                            */
    { "DevilMayCry5.exe", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Call of Duty WW2                           */
    { "s2_sp64_ship.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Need for Speed 2015                        */
    { "NFS16.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mass Effect Andromeda                      */
    { "MassEffectAndromeda.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mirror`s Edge Catalyst: Crashes on AMD     */
    { "MirrorsEdgeCatalyst.exe", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Mirror`s Edge Catalyst Trial               */
    { "MirrorsEdgeCatalystTrial.exe", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Star Wars Battlefront (2015)               */
    { "starwarsbattlefront.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Star Wars Battlefront (2015) Trial         */
    { "starwarsbattlefronttrial.exe", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Dark Souls Remastered                      */
    { "DarkSoulsRemastered.exe", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Grim Dawn                                  */
    { "Grim Dawn.exe", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* NieR:Automata                              */
    { "NieRAutomata.exe", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* The Surge                                  */
    { "TheSurge.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" },
    }} },

    /**********************************************/
    /* D3D9 GAMES                                 */
    /**********************************************/

    /* A Hat in Time                              */
    { "HatinTimeGame.exe", {{
      { "d3d9.strictPow",                   "False" },
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Risen                                      */
    { "Risen.exe", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
    }} },
    /* Risen 2                                    */
    { "Risen2.exe", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
    }} },
    /* Risen 3                                    */
    { "Risen3.exe", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
    }} },
    /* Star Wars: The Force Unleashed 1 & 2       */
    { "SWTFU.exe", {{
      { "d3d9.hasHazards",                  "True" },
    }} },
    { "SWTFU2.exe", {{
      { "d3d9.hasHazards",                  "True" },
    }} },
    /* Grand Theft Auto IV                        */
    { "GTAIV.exe", {{
      { "d3d9.hasHazards",                  "True" },
    }} },
  }};


  static bool isWhitespace(char ch) {
    return ch == ' ' || ch == '\x9' || ch == '\r';
  }

  
  static bool isValidKeyChar(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch == '.' || ch == '_');
  }


  static size_t skipWhitespace(const std::string& line, size_t n) {
    while (n < line.size() && isWhitespace(line[n]))
      n += 1;
    return n;
  }


  struct ConfigContext {
    bool active;
  };


  static void parseUserConfigLine(Config& config, ConfigContext& ctx, const std::string& line) {
    std::stringstream key;
    std::stringstream value;

    // Extract the key
    size_t n = skipWhitespace(line, 0);

    if (n < line.size() && line[n] == '[') {
      n += 1;

      size_t e = line.size() - 1;
      while (e > n && line[e] != ']')
        e -= 1;

      while (n < e)
        key << line[n++];
      
      ctx.active = key.str() == env::getExeName();
    } else {
      while (n < line.size() && isValidKeyChar(line[n]))
        key << line[n++];
      
      // Check whether the next char is a '='
      n = skipWhitespace(line, n);
      if (n >= line.size() || line[n] != '=')
        return;

      // Extract the value
      n = skipWhitespace(line, n + 1);
      while (n < line.size() && !isWhitespace(line[n]))
        value << line[n++];
      
      if (ctx.active)
        config.setOption(key.str(), value.str());
    }
  }


  Config::Config() { }
  Config::~Config() { }


  Config::Config(OptionMap&& options)
  : m_options(std::move(options)) { }


  void Config::merge(const Config& other) {
    for (auto& pair : other.m_options)
      m_options.insert(pair);
  }


  void Config::setOption(const std::string& key, const std::string& value) {
    m_options.insert_or_assign(key, value);
  }


  std::string Config::getOptionValue(const char* option) const {
    auto iter = m_options.find(option);

    return iter != m_options.end()
      ? iter->second : std::string();
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          std::string&  result) {
    result = value;
    return true;
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          bool&         result) {
    if (value == "True") {
      result = true;
      return true;
    } else if (value == "False") {
      result = false;
      return true;
    } else {
      return false;
    }
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          int32_t&      result) {
    if (value.size() == 0)
      return false;
    
    // Parse sign, don't allow '+'
    int32_t sign = 1;
    size_t start = 0;

    if (value[0] == '-') {
      sign = -1;
      start = 1;
    }

    // Parse absolute number
    int32_t intval = 0;

    for (size_t i = start; i < value.size(); i++) {
      if (value[i] < '0' || value[i] > '9')
        return false;
      
      intval *= 10;
      intval += value[i] - '0';
    }

    // Apply sign and return
    result = sign * intval;
    return true;
  }
  
  
  bool Config::parseOptionValue(
    const std::string&  value,
          Tristate&     result) {
    if (value == "True") {
      result = Tristate::True;
      return true;
    } else if (value == "False") {
      result = Tristate::False;
      return true;
    } else if (value == "Auto") {
      result = Tristate::Auto;
      return true;
    } else {
      return false;
    }
  }


  Config Config::getAppConfig(const std::string& appName) {
    auto appConfig = g_appDefaults.find(appName);
    if (appConfig != g_appDefaults.end()) {
      // Inform the user that we loaded a default config
      Logger::info(str::format("Found built-in config: ", appName));

      return appConfig->second;
    }

    return Config();
  }


  Config Config::getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar("DXVK_CONFIG_FILE");

    if (filePath == "")
      filePath = "dxvk.conf";
    
    // Open the file if it exists
    std::ifstream stream(filePath);

    if (!stream)
      return config;
    
    // Inform the user that we loaded a file, might
    // help when debugging configuration issues
    Logger::info(str::format("Found config file: ", filePath));

    // Initialize parser context
    ConfigContext ctx;
    ctx.active = true;

    // Parse the file line by line
    std::string line;

    while (std::getline(stream, line))
      parseUserConfigLine(config, ctx, line);
    
    return config;
  }


  void Config::logOptions() const {
    if (!m_options.empty()) {
      Logger::info("Effective configuration:");

      for (auto& pair : m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));
    }
  }

}
