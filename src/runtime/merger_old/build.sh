g++ -std=c++11 -c NFPacket.h NFPacket.cpp
g++ -std=c++11 -c NF.h NF.cpp
g++ -std=c++11 -c Field.h Field.cpp
g++ -std=c++11 -c RuntimeNode.h RuntimeNode.cpp
g++ -std=c++11 -c packet.h packet.cpp
g++ -std=c++11 -c ActionTableHelper.h ActionTableHelper.cpp
g++ -std=c++11 -c ConflictItem.h ConflictItem.cpp
g++ -std=c++11 -c MergerInfo.h MergerInfo.cpp
g++ -std=c++11 -c MergerOperator.h MergerOperator.h

g++ -std=c++11 NFPacket.cpp NF.cpp Field.cpp packet.cpp RuntimeNode.cpp ActionTableHelper.cpp ConflictItem.cpp MergerInfo.cpp MergerOperator.cpp merge.cpp -o merge -I /usr/include/pcap -lpcap