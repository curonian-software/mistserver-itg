/// \file controller_updater.cpp
/// Contains all code for the controller updater.

#include "controller_updater.h"
#include "controller_connectors.h"
#include "controller_storage.h"
#include <fstream>  //for files
#include <iostream> //for stdio
#include <mist/auth.h>
#include <mist/config.h>
#include <mist/defines.h>
#include <mist/http_parser.h>
#include <mist/timing.h>
#include <signal.h>   //for raise
#include <sys/stat.h> //for chmod
#include <time.h>     //for time
#include <unistd.h>   //for unlink

#define UPDATE_INTERVAL 3600
#ifndef SHARED_SECRET
#define SHARED_SECRET "empty"
#endif

static std::string readFile(std::string filename){
  std::ifstream file(filename.c_str());
  if (!file.good()){return "";}
  file.seekg(0, std::ios::end);
  unsigned int len = file.tellg();
  file.seekg(0, std::ios::beg);
  std::string out;
  out.reserve(len);
  unsigned int i = 0;
  while (file.good() && i++ < len){out += file.get();}
  file.close();
  return out;
}

static bool writeFile(std::string filename, std::string &contents){
  unlink(filename.c_str());
  std::ofstream file(filename.c_str(), std::ios_base::trunc | std::ios_base::out);
  if (!file.is_open()){return false;}
  file << contents;
  file.close();
  chmod(filename.c_str(), S_IRWXU | S_IRWXG);
  return true;
}

tthread::mutex updaterMutex;
uint8_t updatePerc = 0;
JSON::Value updates;

namespace Controller{

  void updateThread(void *np){
    uint64_t updateChecker = Util::epoch() - UPDATE_INTERVAL;
    while (Controller::conf.is_active){
      if (Util::epoch() - updateChecker > UPDATE_INTERVAL || updatePerc){
        JSON::Value result = Controller::checkUpdateInfo();
        if (result.isMember("error")){
          FAIL_MSG("Error retrieving update information: %s",
                   result["error"].asStringRef().c_str());
        }
        {// Lock the mutex, update the updates object
          tthread::lock_guard<tthread::mutex> guard(updaterMutex);
          updates = result;
        }
        if (!result["uptodate"] && updatePerc){
          Socket::Connection updrConn("releases.mistserver.org", 80, true);
          if (!updrConn){
            FAIL_MSG("Could not connect to releases.mistserver.org for update");
          }else{
            // loop through the available components, update them
            unsigned int needCount = result["needs_update"].size();
            if (needCount){
              jsonForEach(result["needs_update"], it){
                if (!Controller::conf.is_active){break;}
                updatePerc = ((it.num() * 99) / needCount) + 1;
                updateComponent(it->asStringRef(), result[it->asStringRef()].asStringRef(),
                                updrConn);
              }
            }
            updrConn.close();
          }
          updatePerc = 0;
        }
        updateChecker = Util::epoch();
      }
      Util::sleep(3000);
    }
  }

  void insertUpdateInfo(JSON::Value &ret){
    tthread::lock_guard<tthread::mutex> guard(updaterMutex);
    ret = updates;
    if (updatePerc){ret["progress"] = (long long)updatePerc;}
  }

  /// Downloads the latest details on updates
  JSON::Value checkUpdateInfo(){
    JSON::Value ret;
    HTTP::Parser http;
    JSON::Value updrInfo;
    // retrieve update information
    Socket::Connection updrConn("releases.mistserver.org", 80, true);
    if (!updrConn){
      Log("UPDR", "Could not connect to releases.mistserver.org to get update information.");
      ret["error"] = "Could not connect to releases.mistserver.org to get update information.";
      return ret;
    }
    http.url = "/getsums.php?verinfo=1&rel=" RELEASE "&pass=" SHARED_SECRET "&iid=" + instanceId;
    http.method = "GET";
    http.SetHeader("Host", "releases.mistserver.org");
    http.SetHeader("X-Version", PACKAGE_VERSION);
    updrConn.SendNow(http.BuildRequest());
    http.Clean();
    unsigned int startTime = Util::epoch();
    while ((Util::epoch() - startTime < 10) && (updrConn || updrConn.Received().size())){
      if (updrConn.spool() && http.Read(updrConn)){
        updrInfo = JSON::fromString(http.body);
        break; // break out of while loop
      }
      Util::sleep(250);
    }
    updrConn.close();

    if (updrInfo){
      if (updrInfo.isMember("error")){
        Log("UPDR", updrInfo["error"].asStringRef());
        ret["error"] = updrInfo["error"];
        ret["uptodate"] = 1;
        return ret;
      }
      ret["release"] = RELEASE;
      if (updrInfo.isMember("version")){ret["version"] = updrInfo["version"];}
      if (updrInfo.isMember("date")){ret["date"] = updrInfo["date"];}
      ret["uptodate"] = 1;
      ret["needs_update"].null();

      // check if everything is up to date or not
      jsonForEach(updrInfo, it){
        if (it.key().substr(0, 4) != "Mist"){continue;}
        ret[it.key()] = *it;
        if (it->asString() != Secure::md5(readFile(Util::getMyPath() + it.key()))){
          ret["uptodate"] = 0;
          if (it.key().substr(0, 14) == "MistController"){
            ret["needs_update"].append(it.key());
          }else{
            ret["needs_update"].prepend(it.key());
          }
        }
      }
    }else{
      Log("UPDR", "Could not retrieve update information from releases server.");
      ret["error"] = "Could not retrieve update information from releases server.";
    }
    return ret;
  }

  /// Causes the updater thread to download an update, if available
  void checkUpdates(){updatePerc = 1;}// CheckUpdates

  /// Attempts to download an update for the listed component.
  /// \param component Filename of the component being checked.
  /// \param md5sum The MD5 sum of the latest version of this file.
  /// \param updrConn An connection to releases.mistserver.org to (re)use. Will be (re)opened if
  /// closed.
  void updateComponent(const std::string &component, const std::string &md5sum,
                       Socket::Connection &updrConn){
    Log("UPDR", "Updating " + component);
    std::string new_file;
    HTTP::Parser http;
    http.url = "/getfile.php?rel=" RELEASE "&pass=" SHARED_SECRET "&file=" + component;
    http.method = "GET";
    http.SetHeader("Host", "releases.mistserver.org");
    http.SetHeader("X-Version", PACKAGE_VERSION);
    if (!updrConn){
      updrConn = Socket::Connection("releases.mistserver.org", 80, true);
      if (!updrConn){
        FAIL_MSG("Could not connect to releases.mistserver.org for file download.");
        return;
      }
    }
    http.SendRequest(updrConn);
    http.Clean();
    uint64_t startTime = Util::bootSecs();
    while ((Util::bootSecs() < startTime + 10) && updrConn && Controller::conf.is_active){
      if (!updrConn.spool()){
        Util::sleep(250);
        continue;
      }
      if (http.Read(updrConn)){
        new_file = http.body;
        break; // break out of while loop
      }
      startTime = Util::bootSecs();
    }
    http.Clean();
    if (new_file == ""){
      FAIL_MSG("Could not retrieve new version of %s, continuing without", component.c_str());
      return;
    }
    if (Secure::md5(new_file) != md5sum){
      FAIL_MSG("Checksum of %s incorrect, continuing without", component.c_str());
      return;
    }
    if (!writeFile(Util::getMyPath() + component, new_file)){
      FAIL_MSG("Could not write updated version of %s, continuing without", component.c_str());
      return;
    }
    Controller::UpdateProtocol(component);
    if (component == "MistController"){
      restarting = true;
      raise(SIGINT); // trigger restart
    }
    Log("UPDR", "New version of " + component + " installed.");
  }
}

