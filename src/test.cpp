
#include <../../simpletest/simpletest.hpp>
#include "persist.h"

class TestPersist : public Test::Fixture<TestPersist>
{
public:
    TestPersist() : base("persist")
    {
        AddTest(&TestPersist::DefaultConstructor);
        AddTest(&TestPersist::EmptyFile);
        AddTest(&TestPersist::Versions);
        AddTest(&TestPersist::TestData);
        AddTest(&TestPersist::TestLimits);
        AddTest(&TestPersist::TestModes);
        AddTest(&TestPersist::TestAllocators);
    }

    void DefaultConstructor()
    {
        persist::map_file file;
        CHECK(!file);
    }

    void EmptyFile()
    {
        persist::map_file file("file.db", 0, 0, 0, 1000, 1000, persist::create_new);
        CHECK(file);
        CHECK(file.data().empty());
        CHECK(file.data().size()==0);
    }
    
    void Versions()
    {
        {
            persist::map_file file("file.db", 0, 0, 0, 1000, 1000, persist::create_new);
        }
        
        {
            // Reopen no problem
            persist::map_file file("file.db", 0, 0, 0);
        }
        
        CheckThrows<persist::InvalidVersion>([]() {
            persist::map_file file("file.db", 1, 0, 0);
        }, __FILE__,__LINE__);

        CheckThrows<persist::InvalidVersion>([]() {
            persist::map_file file("file.db", 0, 1, 0);
        }, __FILE__,__LINE__);

        CheckThrows<persist::InvalidVersion>([]() {
            persist::map_file file("file.db", 0, 0, 1);
        }, __FILE__,__LINE__);
    }
    
    typedef std::basic_string<char, std::char_traits<char>, persist::allocator<char>> pstring;
    typedef std::basic_string<char, std::char_traits<char>, persist::fast_allocator<char>> fstring;

    struct Demo
    {
        int value;
        
        pstring string1;
        fstring string2;
    
        std::shared_ptr<int> intptr;
        std::vector<pstring, persist::allocator<pstring>> vec;
    
        Demo(persist::shared_memory & mem) : string1(mem), string2(mem), vec(persist::allocator<pstring>(mem))
        {
            intptr = std::allocate_shared<int, persist::fast_allocator<int>>(mem, 123);
        }
    };
    
    void TestData()
    {
        persist::map_file file("file.db", 0, 0, 0);
    }
    
    void TestLimits()
    {
        {
            persist::map_file file("file.db", 0,0,0,16384, 16384, persist::create_new);
            TestHeapLimit(file.data(), 16384);
        }

        {
            persist::map_file file("file.db", 0,0,0,16384, 65536, persist::create_new);
            TestHeapLimit(file.data(), 65536);
        }

        {
            persist::map_file file(nullptr, 0,0,0,16384, 16384, persist::temp_heap);
            TestHeapLimit(file.data(), 16384);
        }

        {
            persist::map_file file(nullptr, 0,0,0,16384, 65536, persist::temp_heap);
            TestHeapLimit(file.data(), 65536);
        }
    }
    
    void ValidateMemory(void *p, size_t size)
    {
        char *c=(char*)p;
        for(int i=0; i<size; ++i)
            c[i] = i;

        for(int i=0; i<size; ++i)
            EQUALS((char)i, c[i]);
    }

    void TestHeapLimit(persist::shared_memory &mem, size_t expected_limit)
    {
        auto initial_capacity = mem.capacity();

        auto p = mem.malloc(mem.capacity()/2);
        CHECK(p);
        ValidateMemory(p, initial_capacity/2);
        mem.clear();

        p = mem.malloc(expected_limit);
        CHECK(!p);
        mem.clear();
        EQUALS(initial_capacity, mem.capacity());

        p = mem.malloc(mem.capacity()/2);
        CHECK(p);
        ValidateMemory(p, initial_capacity/2);
        mem.clear();
        
        p = mem.fast_malloc(mem.capacity());
        CHECK(p);
        ValidateMemory(p, initial_capacity);
        CHECK(p);
        mem.clear();
        
        for(int i=0; i<8; ++i)
        {
            p = mem.fast_malloc((initial_capacity/8)&~7);
            CHECK(p);
        }
    }
    
    void TestModes()
    {
        {
            persist::map_file file(nullptr, 0,0,0,16384, 16384, persist::temp_heap);
            CHECK(file);
            
            persist::map_data<Demo> data { file.data(), file.data() };
            data->value = 10;
            
            bool failed = false;
            for(int i=0; i<100; ++i)
            {
                auto p = file.data().malloc(1000);
                if(p)
                    ValidateMemory(p, 1000);
                else
                    failed = true;
            }
            CHECK(failed);
        }

        {
            persist::map_file file("file.db", 0, 0, 0, 16384, 10000, persist::create_new);
            CHECK(file);
            persist::map_data<Demo> data { file.data(), file.data() };
            EQUALS(0, data->value);
            data->value = 10;
        }

        {
            persist::map_file file("file.db", 0, 0, 0, 16384, 10000);
            CHECK(file);
            persist::map_data<Demo> data { file.data(), file.data() };
            EQUALS(10, data->value);
        }

    }

    void TestAllocators()
    {
        persist::map_file file(nullptr, 0,0,0,16384, 1000000, persist::temp_heap);
        CHECK(file);
        
        persist::map_data<Demo> data { file.data(), file.data() };
    }
} tp;

int main()
{
    return 0;
}
