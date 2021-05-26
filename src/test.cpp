
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
        persist::map_file file("file.db", 1000, 1000, persist::create_new);
        CHECK(file);
        CHECK(file.empty());
    }
    
    void Versions()
    {
        // check_throws<persist::error>(
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
    
        Demo(persist::map_file & map) : string1(map), string2(map), vec(persist::allocator<pstring>(map))
        {
            intptr = std::allocate_shared<int, persist::fast_allocator<int>>(map, 123);
        }
    };
    
    void TestData()
    {
        persist::map_file file("file.db");
    }
    
    void TestLimits()
    {
        
    }
    
    void TestModes()
    {
        {
            persist::map_file file(nullptr, 16384, 16384, persist::temp_heap);
            CHECK(file);
            
            persist::map_data<Demo> data { file, file };
            data->value = 10;
            
            bool failed = false;
            for(int i=0; i<100; ++i)
            {
                auto p = file.malloc(1000);
                if(!p) failed = true;
            }
            CHECK(failed);
        }

        {
            persist::map_file file("file.db", 16384, 10000, persist::create_new);
            CHECK(file);
            persist::map_data<Demo> data { file, file };
            EQUALS(0, data->value);
            data->value = 10;
        }

        {
            persist::map_file file("file.db", 16384, 10000);
            CHECK(file);
            persist::map_data<Demo> data { file, file };
            EQUALS(10, data->value);
        }

    }
    

    void TestAllocators()
    {
        persist::map_file file(nullptr, 16384, 1000000, persist::temp_heap);
        CHECK(file);
        
        persist::map_data<Demo> data { file, file };
    }
} tp;

int main()
{
    return 0;
}
