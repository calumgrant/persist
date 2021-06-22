// Copyright (C) Calum Grant 2003
// Copying permitted under the terms of the GNU Public Licence (GPL)
//
// Data stored specific to unix

#include <mutex>

namespace persist
{
    typedef long long offset_t;

    const size_t default_map_address = 0x188000000000ll;

    class shared_base   // unix version
    {
    public:
        int fd;
        std::mutex mem_mutex, user_mutex;
        int mapFlags;
    };

}



