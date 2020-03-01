#include <cstdio>
#include <list>
#include <forward_list>
#include <string>
#include <Loader.h>
#include <minecraft/core/Minecraft.h>
#include <minecraft/core/getSP.h>
#include <minecraft/core/types.h>
#include <minecraft/level/Level.h>
#include <minecraft/actor/Player.h>
#include"../base/base.h"
#include "cmdhelper.h"
#include <chrono>
#include <unordered_map>
#include <cstdarg>
#include <math.h>
#include "MPMCQueue.h"
#include <thread>
#include <fstream>
#include <ctime>
#include "out.h"

const char meta[] __attribute__((used, section("meta"))) =
        "name:packetlogger\n"
        "version:20200203\n"
        "author:extollite\n"
        "depend:base@20200121,command@20200121\n";

using std::string;
using std::deque;
#define min(a, b) ((a)<(b)?(a):(b))
#define max(a, b) ((a)>(b)?(a):(b))
#define dbg_printf(...) {}

//#define dbg_printf printf


extern "C"
{
BDL_EXPORT void mod_init(std::list<string> &modlist);
}

extern void load_helper(std::list<string> &modlist);

struct Packet {
    virtual ~Packet();

    virtual int getId() const = 0;

    virtual std::string getName() const = 0;

    unsigned char getClientSubId() const;
};

struct ReadOnlyBinaryStream {
// void *vtable;
    void* filler[5];
    std::string* data;
    virtual ~ReadOnlyBinaryStream();
};

struct NetEventCallback {
};

class PacketNetLogger {
private:
    static bool asyncThreadRunning;
    std::fstream out;
    static rigtorp::MPMCQueue<std::function<void ()>> queue;
    static std::unordered_map<ServerPlayer *, std::unique_ptr<PacketNetLogger>> loggers;

    void log(std::string const &pkName, const int& id, std::string const &data);

public:
    PacketNetLogger(std::string const &playerName);

    ~PacketNetLogger();

    static void startAsyncThread();

    static void stopAsyncThread();

    static void postAsync(std::function<void()> callback);


    static void registerPlayer(ServerPlayer *player);

    static void unregisterPlayer(ServerPlayer *player);

    static void log(ServerPlayer *player, std::string const &pkName, int const &id, std::string const &data);

    static const unordered_map<ServerPlayer *, std::unique_ptr<PacketNetLogger>> &getLoggers();
};

bool PacketNetLogger::asyncThreadRunning;
rigtorp::MPMCQueue<std::function<void()>> PacketNetLogger::queue(10);
std::unordered_map<ServerPlayer *, std::unique_ptr<PacketNetLogger>> PacketNetLogger::loggers;

void PacketNetLogger::startAsyncThread() {
    asyncThreadRunning = true;
    std::thread thread([]() {
        std::function<void()> cb;
        while (asyncThreadRunning) {
            queue.pop(cb);
            if (cb)
                cb();
        }
    });
    thread.detach();
}

void PacketNetLogger::stopAsyncThread() {
    PacketNetLogger::postAsync([]() {
        asyncThreadRunning = false;
    });
}

void PacketNetLogger::postAsync(std::function<void()> callback) {
    queue.push(std::move(callback));
}

void PacketNetLogger::registerPlayer(ServerPlayer *player) {
    auto playerName = player->getNameTag();
    PacketNetLogger::postAsync([player, playerName]() {
        if (loggers.count(player) > 0)
            return;
        loggers[player] = std::make_unique<PacketNetLogger>(playerName);
    });
}

void PacketNetLogger::unregisterPlayer(ServerPlayer *player) {
    PacketNetLogger::postAsync([player] {
        loggers.erase(player);
    });
}

void PacketNetLogger::log(ServerPlayer *player, std::string const &pkName, int const &id, std::string const &data) {
    PacketNetLogger::postAsync([player, pkName, id, data]() {
        auto i = loggers.find(player);
        if (i == loggers.end())
            return;
        i->second->log(pkName, id, data);
    });
}

PacketNetLogger::PacketNetLogger(const std::string &playerName) {
    std::time_t t = std::time(0);   // get time now
    std::tm* now = std::localtime(&t);
    string day = "log/"+std::to_string(now->tm_year+1900)+"-"+std::to_string(now->tm_mon + 1)+"-"+
                 std::to_string(now->tm_mday);
    mkdir("log", S_IRWXU);
    mkdir(day.c_str(), S_IRWXU);
    string name = day+"/"+playerName;
    mkdir(name.c_str(), S_IRWXU);
    std::string filename = name+"/"+std::to_string(now->tm_hour)+"-"+std::to_string(now->tm_min)+"-"
                                +std::to_string(now->tm_sec)+".log";
    out.open(filename, std::fstream::out | std::fstream::binary);
    if(!out.is_open()){
        do_log("Player file not opened!");
    }
}

PacketNetLogger::~PacketNetLogger() {
    out.close();
}

void PacketNetLogger::log(const std::string &pkName, const int& id, const std::string &data) {
    out << pkName << " " << id << " " << data.size() << std::endl << data << std::endl << std::endl;
}

const unordered_map<ServerPlayer *, std::unique_ptr<PacketNetLogger>> &PacketNetLogger::getLoggers() {
    return loggers;
}

static Packet *savedPacket;
static ReadOnlyBinaryStream *savedStream;
static unsigned char savedSubclient;

static void onJoin(ServerPlayer *sp) {
    if(PacketNetLogger::getLoggers().empty()){
        PacketNetLogger::startAsyncThread();
    }
    PacketNetLogger::registerPlayer(sp);

}

static void onLeft(ServerPlayer* sp){
    PacketNetLogger::unregisterPlayer(sp);
    if(PacketNetLogger::getLoggers().empty()){
        PacketNetLogger::stopAsyncThread();
    }
}

THook(void*, _ZN6Packet12readNoHeaderER20ReadOnlyBinaryStreamRKh, Packet* sh, ReadOnlyBinaryStream &stream,
            unsigned char const &subclient) {
    savedPacket = sh;
    savedStream = &stream;
    savedSubclient = subclient;
    return original(sh, stream, subclient);
}

THook(void* , _ZN6Packet6handleERK17NetworkIdentifierR16NetEventCallbackRSt10shared_ptrIS_E, Packet* sh,
            NetworkIdentifier const &netid, NetEventCallback &callback, std::shared_ptr<Packet> &pk) {
    if (savedPacket == sh) {
        ServerPlayer *player = getMC()->getServerNetworkHandler()->_getServerPlayer(netid, pk.get()->getClientSubId());
        if(player && pk.get()->getName() != "PlayerAuthInputPacket"
        && pk.get()->getName() !="NetworkStackLatencyPacket"
        && pk.get()->getName() != "ClientCacheBlobStatusPacket"
        && pk.get()->getName() != "AnimatePacket"
        && pk.get()->getName() != "LevelSoundEventPacket"
        && pk.get()->getName() != "MovePlayerPacket"){
            PacketNetLogger::log(player, pk.get()->getName(), pk.get()->getId(), std::string(*savedStream->data));
        }
    }
    savedPacket = nullptr;
    return original(sh, netid, callback, pk);
}

void mod_init(std::list<string> &modlist) {
    do_log("loaded! "
                   BDL_TAG);
    reg_player_join(onJoin);
    reg_player_left(onLeft);
    load_helper(modlist);
}

