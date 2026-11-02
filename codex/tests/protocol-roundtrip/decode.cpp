#include "components/ble/GpsData.h"
#include <cstdio>
#include <cstring>
int main(int argc,char** argv) {
  if(argc!=2 || std::strlen(argv[1])!=40) return 2;
  Pinetime::Controllers::GpsData::Packet packet {};
  for(size_t i=0;i<packet.size();++i) {
    unsigned byte=0;
    if(std::sscanf(argv[1]+i*2,"%2x",&byte)!=1) return 3;
    packet[i]=static_cast<uint8_t>(byte);
  }
  Pinetime::Controllers::GpsData::Measurement m;
  if(!Pinetime::Controllers::GpsData::Decode(packet.data(),packet.size(),m)) return 4;
  std::printf("{\"fix\":%u,\"flags\":%u,\"used\":%u,\"view\":%u,\"age\":%u,\"lat\":%ld,\"lon\":%ld,\"alt\":%d,\"hdop\":%u}\n",
    static_cast<unsigned>(m.fix),m.flags,m.satellitesUsed,m.satellitesInView,m.sourceAgeSeconds,
    static_cast<long>(m.latitudeE7),static_cast<long>(m.longitudeE7),m.altitudeMeters,m.hdopCenti);
}
