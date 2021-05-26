
#include <../../simpletest/simpletest.hpp>
#include "persist.h"

class TestPersist : public Test::Fixture<TestPersist>
{
public:
    TestPersist() : base("persist")
    {
        AddTest(&TestPersist::EmptyFile);
    }

    void EmptyFile()
    {
        persist::map_file file1("file1.db");
    }
} tp;

int main()
{
    return 0;
}