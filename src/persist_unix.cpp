
#include "persist.h"
#include "shared_data.h"

#include <iostream>  // tmp
using namespace std; // tmp

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

using namespace persist;


// constructor::map_file
//
// Opens the file to get the file descriptor (fd).  Then calls mmap to map the file to memory.
// It first read the first part of the file, and the might need to remap the file to the 
// location specified by the first invocation of the map.

map_file::map_file(const char *filename, size_t length, size_t limit, int flags, size_t base)
{
    map_address = 0;
    open(filename, length, limit, flags, base);
}

void map_file::open(const char *filename, size_t length, size_t limit, int flags, size_t base)
{
    close();
    // mem_mutex = PTHREAD_MUTEX_INITIALIZER;
    // user_mutex = PTHREAD_MUTEX_INITIALIZER;

    int mapFlags;

    if(flags & private_map) mapFlags = MAP_PRIVATE|MAP_FIXED;
    else mapFlags =  MAP_SHARED|MAP_FIXED;
    
    int fd = -1;
    
    if(flags & temp_heap)
        mapFlags |= MAP_ANON;
    else
    {
        int open_flags = O_RDWR;
        if(flags & create_new) open_flags |= O_TRUNC;
        
        fd = ::open(filename, open_flags, S_IRWXU|S_IRGRP|S_IROTH);
        
        if(fd == -1)
        {
            // File did not exist, so we create it instead

            fd = ::open(filename, O_CREAT|O_RDWR, S_IRWXU|S_IRGRP|S_IROTH);

            if(fd == -1) return;  // Failed to create file

            // Fill up the file with zeros

            char c=0;
            lseek(fd, length-1, SEEK_SET);
            write(fd, &c, 1);
        }
        else if(flags & create_new)
        {
            char c=0;
            lseek(fd, length-1, SEEK_SET);
            write(fd, &c, 1);
        }
    }
    
    // Seek to the end of the file: we need to ensure enough of the file is allocated
    if(base == 0) mapFlags -= MAP_FIXED;

    char *addr = (char*)mmap((char*)base, length, PROT_WRITE|PROT_READ, mapFlags, fd, 0);

    if(addr == MAP_FAILED)
        map_address = 0;
    else
        map_address = (shared_memory*)addr;

    if(base == 0)
    {
        mapFlags += MAP_FIXED;
    }

    if(map_address)
    {
        // Remap the file according to the specifications in the header file

        void *previous_address = map_address->address;
        size_t previous_length = map_address->current_size;

        if(previous_address && (previous_length != length || previous_address != map_address))
        {
             munmap((char*)map_address, length);
             length = previous_length;
             auto base_address = previous_address;

             addr = (char*)mmap((char*)base_address, length, PROT_WRITE|PROT_READ, mapFlags, fd, 0);

             if(addr == MAP_FAILED)
                map_address = 0;
             else
             {
                map_address = (shared_memory*)addr;        
             }

        }
    }

    if(map_address)
    {
        if(map_address->address)
        {
            if(map_address->address != map_address)
            {
                // This is a failure!

                munmap((char*)map_address, length);
                map_address = 0;
            }
        }
        else
        {
            // This is a new address
            map_address->address = map_address;
            map_address->current_size = length;
            map_address->max_size = limit;
            map_address->end = (char*)map_address + length;
            map_address->top = (char*)map_address->root();
            
            pthread_mutex_init(&map_address->extra.mem_mutex, 0);
            pthread_mutex_init(&map_address->extra.user_mutex, 0);
            map_address->extra.mapFlags = mapFlags;

            // This is not needed
            for(int i=0; i<64; ++i) map_address->free_space[i] = 0;
        }
    }

    // Report on where it ended up
    // std::cout << "Mapped to " << map_address << std::endl;

    global = this;
}



map_file::~map_file()
{
    close();
}


void map_file::close()
{
    if(map_address)
    {
        int fd = map_address->extra.fd;
        map_address->unmap();
        ::close(fd);
    }
}


void shared_memory::unmap()
{
    munmap((char*)this, current_size);
}


#define MREMAP 0    // 1 on Linux

void shared_memory::extend_mapping(size_t extra)
{
    // assert(map_address == base_address);
    size_t old_length = current_size;

    if(extra<16384) extra = 16384;
    if(extra < old_length/2) extra = old_length/2;

    size_t new_length = old_length + extra;
    if(new_length > max_size)
        new_length = max_size;

    // extend the file a bit
    char c=0;
    lseek(this->extra.fd, new_length-1, SEEK_SET);
    write(this->extra.fd, &c, 1);

#if MREMAP
    void *new_address = mremap((char*)map_address, old_length, new_length, 0);  // Do NOT relocate it

    if(new_address)
    {
        // remap successful
        length = new_length;
    }
#else

    int mapFlags = this->extra.mapFlags;
    int fd = this->extra.fd;
    
    munmap((char*)this, old_length);

    char *m = (char*)mmap((char*)this, new_length, PROT_WRITE|PROT_READ, mapFlags, fd, 0);

    if(m == MAP_FAILED)
    {
        // Go back to the original map then
        char *m = (char*)mmap((char*)this, old_length, PROT_WRITE|PROT_READ, mapFlags, fd, 0);
        assert(m != MAP_FAILED);
        assert(m == (char*)this);
    }
    else
    {
        assert(m == (char*)this);
        current_size = new_length;
        end = (char*)this + new_length;
    }

#endif
}


bool shared_memory::lock(int ms)
{
    pthread_mutex_lock(&extra.user_mutex);
    return true;
}


void shared_memory::unlock()
{
    pthread_mutex_unlock(&extra.user_mutex);
}

void shared_memory::lockMem()
{
    pthread_mutex_lock(&extra.mem_mutex);
}


void shared_memory::unlockMem()
{
    pthread_mutex_unlock(&extra.mem_mutex);
}

map_file::map_file()
{
    map_address = nullptr;
}
