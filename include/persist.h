// Copyright (C) Calum Grant 2003-2021
// Copying permitted under the terms of the GNU Public Licence (GPL)

#ifndef PERSIST_H
#define PERSIST_H

#ifdef _WIN32
#include "persist_win32.h"
#else
#include "persist_unix.h"
#endif

#include <memory>
#include <cassert>
#include <atomic>

namespace persist
{
    // Exception thrown when an invalid datafile is opened,
    // or the datafile is the wrong version.
    class InvalidVersion : public std::runtime_error
    {
    public:
        InvalidVersion();
    };

    class shared_memory
    {
    public:
        typedef std::size_t size_type;
        
        // Returns true if the heap is empty: no objects have yet been created
        bool empty() const;
        
        bool lock(int ms=0);    // Mutex the entire heap
        void unlock();          // Release the entire heap

        bool wait(int ms=0);    // Wait for event
        void signal();          // Signal event

        void *root();     // The root object
        const void *root() const;     // The root object
        
        void *malloc(size_t);
        void free(void*, size_t);
        
        void clear();
        
        size_type capacity() const;
        size_type size() const;
        size_type limit() const;
        void limit(size_type);

        void *fast_malloc(size_t size)
        {
            auto r = size&7;
            if(r)
                size += (8-r);
            assert((size&7)==0);
            auto result = top += size;
            if(result > end)
            {
                lockMem();
                bool failed = !extend_to(result);
                if(failed) top -= size;
                unlockMem();
                if(failed) return nullptr;
            }
            // top is a std::atmoic, so we need to do it like this:
            return result - size;
        }

    private:
        friend class map_file;
        shared_memory(const shared_memory&) = delete;
        
        // Magic bytes to check we have loaded the correct version
        int magic;
        int applicationId;
        short majorVersion;
        short minorVersion;
        int hardwareId;
        
        shared_memory *address;   // The address we expect to be at - we need to reopen if this fails

        size_t current_size;          // The size of the allocation
        size_t max_size;

        void *condition;

        std::atomic<char *> top, end;

        void *free_space[64];   // An embarrassingly simple memory manager
        
        shared_base extra;
        
        bool extend_to(void *newTop);
        void unmap();
        void lockMem();
        void unlockMem();
    };


    enum { shared_heap=1, private_map=2, temp_heap=8, create_new=16, read_only=32 };

    // map_file
    // A wrapper around a block of shared memory.
    // This provides memory management functions, locking, and
    // extends the heap when necessary.
    class map_file
    {
        shared_memory *map_address;

    public:

        map_file();
        
        map_file(const char *filename,
                int applicationId,
                 short majorVersion,
                 short minorVersion,
            size_t length=16384,
            size_t limit=1000000,
            int flags = 0,
            size_t base=default_map_address);   

        ~map_file();

        void open(const char *filename, 
                  int applicationId,
                   short majorVersion,
                   short minorVersion,
            size_t length=16384,
            size_t limit=1000000,
            int flags = 0,
            size_t base=default_map_address);

        void close();

        // Returns true if the heap is valid and usable
        operator bool() const { return map_address!=0; }
        
        shared_memory &data() const;
    };

    template<class T>
    class fast_allocator : public std::allocator<T>
    {
    public:
        fast_allocator(map_file & map) : map(map.data()) { }
        fast_allocator(shared_memory & mem) : map(mem) { }

        // Construct from another allocator
        template<class O>
        fast_allocator(const fast_allocator<O>&o) : map(o.map) { }

        typedef T value_type;
        typedef const T *const_pointer;
        typedef T *pointer;
        typedef const T &const_reference;
        typedef T &reference;
        typedef typename std::allocator<T>::difference_type difference_type;
        typedef typename std::allocator<T>::size_type size_type;

        pointer allocate(size_type n)
        {
            pointer p = static_cast<pointer>(map.fast_malloc(n * sizeof(T)));
            if(!p) throw std::bad_alloc();

            return p;
        }

        void deallocate(pointer p, size_type count)
        {
        }

        size_type max_size() const
        {
            return map.capacity()/sizeof(T);
        }

        template<class Other>
        struct rebind
        {
            typedef fast_allocator<Other> other;
        };
        
        shared_memory & map;
    };


    template<class T>
    class allocator : public std::allocator<T>
    {
    public:
        allocator(map_file & map) : map(map.data()) { }
        allocator(shared_memory & mem) : map(mem) { }

        // Construct from another allocator
        template<class O>
        allocator(const allocator<O>&o) : map(o.map) { }

        typedef T value_type;
        typedef const T *const_pointer;
        typedef T *pointer;
        typedef const T &const_reference;
        typedef T &reference;
        typedef typename std::allocator<T>::difference_type difference_type;
        typedef typename std::allocator<T>::size_type size_type;

        pointer allocate(size_type n)
        {
            pointer p = static_cast<pointer>(map.malloc(n * sizeof(T)));
            if(!p) throw std::bad_alloc();

            return p;
        }

        void deallocate(pointer p, size_type count)
        {
            map.free(p, count * sizeof(T));
        }

        size_type max_size() const
        {
            return map.capacity()/sizeof(T);
        }

	    template<class Other>
		struct rebind
		{
            typedef allocator<Other> other;
		};
    
        shared_memory & map;
    };

    template<class T>
    class map_data
    {
    public:
        typedef T value_type;
        
        template<typename... ConstructorArgs>
        map_data(shared_memory & mem, ConstructorArgs&&... init) : file(mem)
        {
            if(mem.empty())
            {                
                new(file) value_type(init...);
            }
        }

        map_data(shared_memory & mem) : file(mem)
        {
            if(this->file.empty())
            {
                new(file) value_type();
            }
        }
        
        value_type &operator*()
        {
            return *static_cast<T*>(file.root());
        }

        const value_type &operator*() const
        {
            return *static_cast<T*>(file.root());
        }
        
        value_type *operator->()
        {
            return static_cast<T*>(file.root());
        }

        const value_type *operator->() const
        {
            return static_cast<T*>(file.root());
        }

    private:
        shared_memory & file;
    };
}


void *operator new(size_t size, persist::shared_memory & mem);
void operator delete(void *p, persist::shared_memory & mem);

#endif
