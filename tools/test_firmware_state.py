"""Compile the actual firmware state handlers against deterministic I/O."""
import pathlib
import re
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = (ROOT / 'src/main.cpp').read_text()


def body(marker):
    start = SOURCE.index('{', SOURCE.index(marker))
    depth = 1
    end = start + 1
    while depth:
        depth += (SOURCE[end] == '{') - (SOURCE[end] == '}')
        end += 1
    return SOURCE[start:end]


def definition(signature):
    return '\n' + signature + ' ' + body(signature + ' {')


def wifi_prelude():
    """Fakes for the radio, stores and server, with the firmware's own Wi-Fi constants and state."""
    constants = ''.join(line + '\n' for line in re.findall(
        r'^const (?:unsigned long|uint8_t) (?:MAX_WIFI_NETWORKS|WIFI_\w+) = \d+;$', SOURCE, re.M))
    state = SOURCE[SOURCE.index('unsigned long wifiDisconnectedSince'):SOURCE.index('unsigned long wifiRetryAt = 0;')]
    return r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "src/platform/PreferenceWrites.h"
using String=std::string;
uint32_t stateRevision=0;
unsigned long clockMs=0;
constexpr int WL_CONNECTED=3,WIFI_MODE_NULL=0,WIFI_STA=1;
bool isAPMode=false;
unsigned long millis(){return clockMs;}
void delay(unsigned long){}
template<class T> T constrain(T value,T low,T high){return value<low?low:value>high?high:value;}
// Erasures, saves, restarts and radio actions in the order they happen.
std::vector<std::string> events;
struct Wifi {
 int statusValue=WL_CONNECTED,stations=0;uint32_t station=1,ap=2;
 int status(){return statusValue;}
 uint32_t localIP(){return station;}
 uint32_t softAPIP(){return ap;}
 int softAPgetStationNum(){return stations;}
 bool mode(int){return true;}
 void setSleep(bool){}
 void setHostname(const char*){}
 void begin(const char* ssid,const char*){events.push_back(std::string("join ")+ssid);}
 void disconnect(bool,bool){events.push_back("stop joining");}
 void softAPdisconnect(bool){events.push_back("close setup");}
 void setAutoReconnect(bool){}
} WiFi;
struct Store {
 bool opens=true,clears=true;std::string opened;std::map<std::string,int> ints;
 bool begin(const char* name,bool){opened=name;return opens;}
 bool clear(){events.push_back("clear "+opened);return clears;}
 int getInt(const char* key,int fallback){auto found=ints.find(key);return found==ints.end()?fallback:found->second;}
 void end(){}
} preferences;
struct Dns {void stop(){}} dnsServer;
struct Esp {void restart(){events.push_back("restart");}} ESP;
void persistSettledSettings(bool immediately=false){events.push_back(immediately?"save now":"save when due");}
struct Text {std::string value;bool startsWith(const char* prefix) const {return value.rfind(prefix,0)==0;}};
struct Server {std::string contentType="application/json";Text header(const char*){return {contentType};}} server;
struct Port {
 std::vector<std::string> lines;
 void print(const char*){}
 void printf(const char*,...){}
 void println(const char* text){lines.emplace_back(text);}
 void println(uint32_t){}
 void flush(){}
} Serial;
bool responseSuccess=false;int responseStatus=0;
void sendJson(bool success,const char*,int status=200){responseSuccess=success;responseStatus=status;}
bool readWifiNetwork(uint8_t index,String& ssid,String& password){
 const char* names[]={"Home","Studio"};if(index>1)return false;
 ssid=names[index];password="secret";return true;
}
bool preferNetwork(int slot){events.push_back("prefer "+std::to_string(slot));return true;}
bool mdnsStarted=false;const char* HOSTNAME="matrix-16";
void configTzTime(const char*,const char*,const char*){}
struct Mdns {bool begin(const char*){events.push_back("mdns");return true;} void addService(const char*,const char*,int){}} MDNS;
void setupOta(){events.push_back("ota");}
''' + constants + state + r'''unsigned long wifiRetryAt = 0;
void startAccessPoint(){isAPMode=true;wifiRetryPosition=0;wifiRetryAt=millis();events.push_back("open setup");}
''' + ''.join(definition(signature) for signature in [
        'void startNetworkServices()', 'int savedNetworkAt(int position, int preferred, int count)',
        'bool joinNextSavedNetwork()', 'void startWifi()', 'void joinedSavedNetwork()',
        'void holdWifiRetries()', 'void retrySavedNetworks()'])


def run(name, fixture):
    work = ROOT / '.local/tests'
    work.mkdir(parents=True, exist_ok=True)
    cpp = work / (name + '.cpp')
    binary = work / name
    cpp.write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I' + str(ROOT), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


class FirmwareWifiTest(unittest.TestCase):
    def test_revisions_and_reset_failures(self):
        fixture = wifi_prelude()
        for signature in ['void restartDevice()', 'bool forgetWifiNetworks()', 'bool jsonRequest()',
                          'void maintainWifi()', 'void handleWifiReset()']:
            fixture += definition(signature)
        fixture += '\nvoid usbReset() ' + body('if (!strcmp(line, "WIFI,RESET")) {')
        fixture += r'''
int main(){
 maintainWifi();assert(stateRevision==1);
 clockMs=250;maintainWifi();assert(stateRevision==1);
 WiFi.statusValue=6;WiFi.station=0;clockMs=500;maintainWifi();assert(stateRevision==2);
 clockMs=1000;maintainWifi();assert(stateRevision==2);
 WiFi.statusValue=WL_CONNECTED;WiFi.station=3;clockMs=1250;maintainWifi();assert(stateRevision==3);
 WiFi.station=4;clockMs=1500;maintainWifi();assert(stateRevision==4);
 WiFi.statusValue=6;WiFi.station=0;clockMs=1750;maintainWifi();assert(stateRevision==5);
 clockMs=61750;maintainWifi();assert(isAPMode);
 clockMs=62000;maintainWifi();assert(stateRevision==6);
 clockMs=62250;maintainWifi();assert(stateRevision==6);
 WiFi.ap=5;clockMs=62500;maintainWifi();assert(stateRevision==7);
 const std::vector<std::string> restart{"save now","restart"},erase{"clear wifi-list"},eraseAndRestart{"clear wifi-list","save now","restart"};
 assert((events==std::vector<std::string>{"open setup"}));events.clear();
 restartDevice();assert(events==restart);
 for(auto reset:{handleWifiReset,usbReset}){
  uint32_t revision=stateRevision;preferences.opens=false;preferences.clears=true;events.clear();
  reset();assert(events.empty()&&stateRevision==revision+1);
  preferences.opens=true;preferences.clears=false;events.clear();
  reset();assert(events==erase);
  preferences.clears=true;events.clear();
  reset();assert(events==eraseAndRestart);
 }
 assert(Serial.lines.back()=="WIFI,RESET,restarting");
 events.clear();handleWifiReset();assert(responseSuccess&&responseStatus==200&&events==eraseAndRestart);
 preferences.clears=false;handleWifiReset();assert(!responseSuccess&&responseStatus==500);
 usbReset();assert(Serial.lines.back()=="WIFI,RESET,failed");
 uint32_t revision=stateRevision;server.contentType="text/plain";preferences.clears=true;events.clear();
 handleWifiReset();assert(!responseSuccess&&responseStatus==415&&events.empty()&&stateRevision==revision);
}
'''
        run('wifi-state', fixture)

    def test_setup_network_retries_saved_networks(self):
        fixture = wifi_prelude() + definition('void maintainWifi()') + r'''
int main(){
 // Every round visits each saved slot once, the one joined last first, in order otherwise.
 for(int count=0;count<=8;count++)for(int preferred=-1;preferred<=10;preferred++){
  std::vector<int> order,expected;
  for(int position=0,slot;(slot=savedNetworkAt(position,preferred,count))>=0;position++)order.push_back(slot);
  if(preferred>=0&&preferred<count)expected.push_back(preferred);
  for(int slot=0;slot<count;slot++)if(slot!=preferred)expected.push_back(slot);
  assert(order==expected);
 }
 preferences.ints={{"count",2},{"preferred",1}};
 WiFi.statusValue=6;startAccessPoint();events.clear();
 auto at=[](unsigned long ms){clockMs=ms;maintainWifi();};
 // A round tries the network joined last, then the other, one timeout each, and pauses a minute.
 at(59999);assert(events.empty());
 at(60000);at(66999);at(67000);at(73999);at(74000);
 assert((events==std::vector<std::string>{"join Studio","join Home","stop joining"}));
 // A phone on the setup network stops an attempt at once, even one that has just joined,
 // and holds the next round until a minute after it leaves.
 events.clear();WiFi.stations=1;at(134000);assert(events.empty());
 WiFi.stations=0;at(194000);WiFi.stations=1;WiFi.statusValue=WL_CONNECTED;at(194100);WiFi.statusValue=6;at(195000);
 assert((events==std::vector<std::string>{"join Studio","stop joining"})&&isAPMode);
 events.clear();WiFi.stations=0;at(254999);assert(events.empty());
 // A scan from the setup page stops an attempt the same way before it starts.
 at(255000);clockMs=256000;holdWifiRetries();at(315999);
 assert((events==std::vector<std::string>{"join Studio","stop joining"}));
 // A network that answers closes the setup network, becomes the one tried first and starts the network services.
 events.clear();at(316000);at(323000);WiFi.statusValue=WL_CONNECTED;at(323100);
 assert((events==std::vector<std::string>{"join Studio","join Home","close setup","prefer 0","mdns","ota"}));
 assert(!isAPMode);at(400000);assert(events.size()==6&&wifiDisconnectedSince==0);
}
'''
        run('wifi-retries', fixture)

    def test_boot_joins_in_the_background(self):
        fixture = wifi_prelude() + definition('void maintainWifi()') + r'''
int main(){
 auto at=[](unsigned long ms){clockMs=ms;maintainWifi();};
 // Without saved networks the setup network opens at once.
 startWifi();assert((events==std::vector<std::string>{"open setup"})&&isAPMode&&!wifiJoining);
 // Boot starts the network joined last and returns at once; each further one
 // gets a connection timeout, and the setup network opens when none answers.
 isAPMode=false;events.clear();preferences.ints={{"count",2},{"preferred",1}};WiFi.statusValue=6;
 clockMs=1000;startWifi();assert((events==std::vector<std::string>{"join Studio"})&&wifiJoining&&!isAPMode);
 at(7999);assert(events.size()==1);
 at(8000);at(14999);assert((events==std::vector<std::string>{"join Studio","join Home"}));
 at(15000);assert((events==std::vector<std::string>{"join Studio","join Home","stop joining","open setup"})&&isAPMode&&!wifiJoining);
 // A network that answers during the boot round becomes the one tried first and starts the services.
 isAPMode=false;events.clear();clockMs=20000;startWifi();
 at(27000);WiFi.statusValue=WL_CONNECTED;at(27100);
 assert((events==std::vector<std::string>{"join Studio","join Home","prefer 0","mdns","ota"})&&!wifiJoining&&!isAPMode);
 // The network joined last answering first writes nothing.
 preferences.ints={{"count",2},{"preferred",0}};mdnsStarted=false;events.clear();WiFi.statusValue=6;clockMs=30000;startWifi();
 WiFi.statusValue=WL_CONNECTED;at(30100);assert((events==std::vector<std::string>{"join Home","mdns","ota"}));
 // Once joined, a lost network opens the setup network after a minute.
 events.clear();WiFi.statusValue=6;at(40000);at(99999);assert(!isAPMode);at(100000);assert(isAPMode);
}
'''
        run('wifi-boot', fixture)


class FirmwareWifiScanTest(unittest.TestCase):
    def test_scans_run_in_the_background_and_lists_stay_fresh(self):
        fixture = r'''
#include <cassert>
#include <string>
using String=std::string;
constexpr int WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;
unsigned long clockMs=0;
unsigned long millis(){return clockMs;}
bool isAPMode=false;int holds=0;
void holdWifiRetries(){holds++;}
// The radio: a scan runs until the test finishes it with a count or a failure.
struct Wifi {
 int result=WIFI_SCAN_FAILED,scans=0;bool starts=true;
 int scanComplete(){return result;}
 int scanNetworks(bool,bool){scans++;result=starts?WIFI_SCAN_RUNNING:WIFI_SCAN_FAILED;return result;}
} WiFi;
// Collecting a list deletes the radio's results, as WiFi.scanDelete() does.
int collected=0;
String wifiScanJson(int found){WiFi.result=WIFI_SCAN_FAILED;return "list"+std::to_string(found)+"#"+std::to_string(++collected);}
struct Server {
 bool again=false;std::string body;
 bool hasArg(const char*){return again;}
 void send(int,const char*,const String& reply){body=reply;}
} server;
''' + SOURCE[SOURCE.index('String wifiScanList;'):SOURCE.index('void handleWifiScan() {')] + definition('void handleWifiScan()') + r'''
int main(){
 auto ask=[](bool again=false){server.again=again;handleWifiScan();return server.body;};
 const std::string scanning="{\"success\":true,\"scanning\":true}",failed="{\"success\":false,\"networks\":[]}";
 // The first request starts a scan, and polls wait for its list.
 assert(ask()==scanning&&WiFi.scans==1);
 clockMs=3000;assert(ask()==scanning);
 WiFi.result=4;clockMs=6000;assert(ask()=="list4#1");
 // Another page waiting on the same scan receives the same list.
 clockMs=20000;assert(ask()=="list4#1"&&WiFi.scans==1);
 // Scan again always scans, and a list 30 s after its scan began is scanned again.
 clockMs=21000;assert(ask(true)==scanning&&WiFi.scans==2);
 WiFi.result=5;clockMs=25000;assert(ask()=="list5#2");
 clockMs=51000;assert(ask()==scanning&&WiFi.scans==3);
 // A list nobody collected in time is never shown as fresh, with or without ?again.
 WiFi.result=6;clockMs=200000;assert(ask()==scanning&&WiFi.scans==4&&collected==3);
 WiFi.result=7;clockMs=203000;assert(ask(true)==scanning&&WiFi.scans==5&&collected==4);
 // A scan that times out fails once; one that cannot start fails and leaves no list behind.
 WiFi.result=WIFI_SCAN_FAILED;clockMs=204000;assert(ask()==failed);
 WiFi.starts=false;assert(ask()==failed&&WiFi.scans==6);
 WiFi.starts=true;assert(ask()==scanning&&WiFi.scans==7);
 // On the setup network a scan first stops a station attempt.
 isAPMode=true;WiFi.result=2;clockMs=300000;assert(ask(true)==scanning&&holds==1&&WiFi.scans==8);
}
'''
        run('wifi-scan', fixture)


class FirmwarePatternRecordTest(unittest.TestCase):
    def test_edits_survive_failed_saves_until_storage_recovers(self):
        fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "src/patterns/PatternCatalog.h"
#include "src/patterns/PatternStore.h"
unsigned long clockMs=0,lastAnimationChangeTime=0;
unsigned long millis(){return clockMs;}
// The matrix-cfg namespace, whose writes fail while storage is broken.
std::map<std::string,std::vector<uint8_t>> blobs;
bool storageWorks=true;int writes=0;
struct Preferences {
 bool begin(const char*,bool){return true;}
 void end(){}
 bool isKey(const char* key){return blobs.count(key)>0;}
 size_t getBytesLength(const char* key){return isKey(key)?blobs[key].size():0;}
 size_t getBytes(const char* key,void* data,size_t size){
  const auto& bytes=blobs[key];if(size<bytes.size())return 0;
  memcpy(data,bytes.data(),bytes.size());return bytes.size();
 }
 size_t putBytes(const char* key,const void* data,size_t size){
  writes++;if(!storageWorks)return 0;
  blobs[key].assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size);return size;
 }
 size_t putUChar(const char*,uint8_t){return 1;}
};
enum Animation {STARFIELD,PATTERNFLOW_COLLECTION=16};
Animation currentAnimation=STARFIELD;
struct ColorParams {uint16_t saturation;uint8_t palette;};
struct PatternflowCollectionParams {uint8_t variant,control[4];};
uint8_t starfield[4];PatternflowCollectionParams pfcParams;
ColorParams colorParams[17];uint32_t paletteStops[17][4];
const PatternflowMetadata PATTERNFLOW_METADATA[PATTERNFLOW_PATTERN_COUNT]={{"Shown",{"a","b","c","d"}}};
size_t engineControls(int engine,void*& controls){
 if(engine==PATTERNFLOW_COLLECTION){controls=&pfcParams;return sizeof(pfcParams);}
 controls=starfield;return sizeof(starfield);
}
bool brightnessDirty=false;unsigned long brightnessSaveDueMs=0;uint8_t deviceBrightness=3;
void persistSettledSettings(bool immediately=false);
'''
        fixture += SOURCE[SOURCE.index('struct PatternLook {'):SOURCE.index('bool readPatternRecord(')]
        for signature in ['bool readPatternRecord(int engine, int variant)',
                          'size_t buildPatternRecord(uint8_t (&record)[PATTERN_RECORD_CAPACITY])',
                          'bool savePatternRecord()', 'bool savePendingRecords()', 'void markConfigEdited()',
                          'bool saveCurrentConfig()', 'void flushConfig()',
                          'void persistSettledSettings(bool immediately)']:
            fixture += definition(signature)
        fixture += r'''
// Leaves the playing pattern as startPattern does, then loads the next from its defaults and record.
void play(int engine){
 flushConfig();currentAnimation=(Animation)engine;
 void* controls;const size_t size=engineControls(engine,controls);memset(controls,0,size);
 readPatternRecord(engine,0);
}
// A control byte stored under key, or -1 without a record.
int stored(const char* key,size_t offset){return blobs.count(key)?blobs[key][sizeof(PatternLook)+offset]:-1;}
int main(){
 play(PATTERNFLOW_COLLECTION);pfcParams.control[0]=7;markConfigEdited();
 storageWorks=false;
 play(STARFIELD);starfield[0]=9;markConfigEdited();
 play(PATTERNFLOW_COLLECTION);assert(pfcParams.control[0]==7);
 pfcParams.control[0]=8;markConfigEdited();
 play(STARFIELD);assert(starfield[0]==9);
 // Retries wait 30 s after a failure.
 const int failed=writes;clockMs=29999;persistSettledSettings();assert(writes==failed);
 clockMs=30000;persistSettledSettings();assert(writes>failed&&blobs.empty()&&configDirty&&!pendingRecords.empty());
 storageWorks=true;clockMs=59999;persistSettledSettings();assert(blobs.empty());
 clockMs=60000;persistSettledSettings();
 assert(!configDirty&&pendingRecords.empty()&&stored("p16",1)==8&&stored("p0",0)==9);
 // A restart stores held edits without waiting.
 storageWorks=false;starfield[0]=5;markConfigEdited();
 play(PATTERNFLOW_COLLECTION);assert(pfcParams.control[0]==8&&!pendingRecords.empty());
 storageWorks=true;persistSettledSettings(true);assert(pendingRecords.empty()&&stored("p0",0)==5);
 play(STARFIELD);assert(starfield[0]==5);
}
'''
        run('pattern-records', fixture)


if __name__ == '__main__':
    unittest.main()
