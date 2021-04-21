#include <cstdlib>
#include <fstream>
#include <map>
#include <mist/bitfields.h>
#include <mist/config.h>
#include <mist/defines.h>
#include <mist/dtsc.h>
#include <mist/encryption.h>
#include <mist/json.h>
#include <mist/shared_memory.h>
#include <mist/timing.h>
#include <set>

#include "../io.h"

namespace Mist{
  struct booking{
    uint32_t first;
    uint32_t curKey;
    uint32_t curPart;
  };

  class Input : public InOutBase{
  public:
    Input(Util::Config *cfg);
    virtual int run();
    virtual void onCrash(){}
    virtual int boot(int argc, char *argv[]);
    virtual ~Input(){};

    bool keepAlive();
    void reloadClientMeta();
    bool hasMeta() const;
    static Util::Config *config;
    virtual bool needsLock(){return !config->getBool("realtime");}

  protected:
    virtual bool checkArguments() = 0;
    virtual bool readHeader();
    virtual bool needHeader(){return !readExistingHeader();}
    virtual bool preRun(){return true;}
    virtual bool isSingular(){return !config->getBool("realtime");}
    virtual bool readExistingHeader();
    virtual bool atKeyFrame();
    virtual void getNext(size_t idx = INVALID_TRACK_ID){}
    virtual void seek(uint64_t seekTime, size_t idx = INVALID_TRACK_ID){}
    virtual void finish();
    virtual bool keepRunning();
    virtual bool openStreamSource(){return readHeader();}
    virtual void closeStreamSource(){}
    virtual void parseStreamHeader(){}
    void checkHeaderTimes(std::string streamFile);
    virtual void removeUnused();
    virtual void convert();
    virtual void serve();
    virtual void stream();
    virtual size_t streamByteCount(){
      return 0;
    }; // For live streams: to update the stats with correct values.
    virtual std::string streamMainLoop();
    virtual std::string realtimeMainLoop();
    bool isAlwaysOn();

    virtual void userLeadIn();
    virtual void userOnActive(size_t id);
    virtual void userOnDisconnect(size_t id);
    virtual void userLeadOut();

    virtual void parseHeader();
    bool bufferFrame(size_t track, uint32_t keyNum);

    uint64_t activityCounter;

    JSON::Value capa;

    int64_t timeOffset;
    std::map<size_t, std::set<uint64_t> > keyTimes;

    // Create server for user pages
    Comms::Users users;
    size_t connectedUsers;

    Encryption::AES aesCipher;

    IPC::sharedPage streamStatus;

    std::map<size_t, std::map<uint32_t, size_t> > pageCounter;

    static Input *singleton;

    bool hasSrt;
    std::ifstream srtSource;
    unsigned int srtTrack;

    void readSrtHeader();
    void getNextSrt(bool smart = true);
    DTSC::Packet srtPack;

    uint64_t simStartTime;

    void handleBuyDRM();
  };
}// namespace Mist
