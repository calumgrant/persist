// Copyright (C) Calum Grant 2003
// Copying permitted under the terms of the GNU Public Licence (GPL)

#include "persist.h"
#include "shared_data.h"

#include <cassert>
#include <iostream>  // Debug only

using namespace persist;

// Whether to reuse freed memory (yes, you want to do this)
#define RECYCLE 1 

// Whether to report memory allocations and deallocations
#define TRACE_ALLOCS 0

// Whether to include extra debugging information (slightly slower, bigger heap)
#define CHECK_MEM 0


// operator new
//
// Allocates space for one object in the shared memory

void *operator new(size_t size, persist::shared_memory &file)
{
    void *p = file.malloc(size);

    if(!p) throw std::bad_alloc();

    return p;
}


// operator delete
//
// Matches operator new.  Not used.

void operator delete(void *p, persist::shared_memory &file)
{
}



// object_cell
//
// "free_space" is a table of free blocks.  We round the size up using
// object_cell() into 64 discrete sizes, 4, 6, 8, 12, 16, 24, 32 ...
//
// Returns the cell number, and also rounds req_size up to the cell size

inline int object_cell(size_t &req_size)
{
    int cell=0;
    size_t cell_size=sizeof(void*);

    // TODO: A more efficient way of extracting just the top two bits of a number

    while(cell<64)  // NB that's just 32 bits!
    {
        size_t s0 = cell_size>>1;

        if(req_size <= cell_size) { req_size = cell_size; return cell; }
        cell++;        
        cell_size += s0; 

        if(req_size <= cell_size) { req_size = cell_size; return cell; }
        cell++;
        cell_size += s0;
    }

    return 0;   // Failure
}


// map_file::malloc
//
// Allocates an object of size @size from the shared memory
// If possible, use a block in the free_space instead of growing the heap.
// Mutexed, threadsafe - very important.

void *shared_memory::malloc(size_t size)
{
    if(size==0) return top;  // A valid address?  TODO

    lockMem();
    
    int free_cell = object_cell(size);

#if RECYCLE   
    if(free_space[free_cell])
    {
        // We have a free cell of the desired size

        void *block = free_space[free_cell];
        free_space[free_cell] = *(void**)block;

#if CHECK_MEM
        ((int*)block)[-1] = size;
#endif

#if TRACE_ALLOCS
        std::cout << " +" << block << "(" << size << ")";
#endif

        unlockMem();
        return block;
    }
#endif

#if CHECK_MEM
    *(int*)map_address->top = size;
    map_address->top += sizeof(int);
#endif

    void *t = top;
        
    auto new_top = top + size;

    if(new_top > end)
    {
        if(max_size <= current_size || !extend_to(new_top))
        {
            unlockMem();
            return nullptr;
        }
    }

    top = new_top;

#if TRACE_ALLOCS
    std::cout << " +" << t << "(" << size << ")";
#endif

    unlockMem();
    return t;
}


// map_file::free
//
// Marks the given memory block as "free"
// Free blocks are stored in a linked list, starting at the vector free_cell.
// The minimum allocation size is 4 bytes to accomodate the pointer

void shared_memory::free(void* block, size_t size)
{
    lockMem();

#if TRACE_ALLOCS
    std::cout << " -" << block << "(" << size << ")";
#endif
    if(size==0) return;  // Do nothing    

    if(block < this || block >= end)
    {
        // We have attempted to "free" data not allocated by this memory manager
        // This is a serious fault, but we carry on

        std::cout << "Block out of range!\n";  // This is a serious error!

        // This happens in basic_string...
        unlockMem();
        return;
    }

    assert(block>=this && block<end);
        // This means that the address is not managed by this heap!

#if CHECK_MEM
        assert(((int*)block)[-1] == size);
        ((int*)block)[-1] = 0;  // This is now DEAD!
#endif

#if RECYCLE   // Enable this to enable block to be reused
    int free_cell = object_cell(size);
    // free_cell is the cell number for blocks of size "size"

    // Add the free block to the linked list in free_space
    *(void**)block = free_space[free_cell];
    free_space[free_cell] = block;
#endif

    unlockMem();
}

// map_file::root
//
// Returns a pointer to the first object in the heap.

void *shared_memory::root()
{
    return this+1;
}

const void *shared_memory::root() const
{
    return this+1;
}


// map_file::empty
//
// Returns true of the heap is empty - no objects have been allocated.
// This tells us if we need to construct a root object.

bool shared_memory::empty() const
{
    return root() == top;  // No objects allocated
}

shared_memory & map_file::data() const
{
    return *map_address;
}

void shared_memory::clear()
{
    top = (char*)root();
    for(int i=0; i<64; ++i)
        free_space[i] = nullptr;
}

size_t shared_memory::capacity() const
{
    return (end-top) + (max_size - current_size);
}

size_t shared_memory::size() const
{
    return top-(char*)root();
}

size_t shared_memory::limit() const
{
    return max_size;
}

void shared_memory::limit(size_t size)
{
    max_size = size;
}

InvalidVersion::InvalidVersion() : std::runtime_error("Version number mismatch")
{
}
