#include <gtest/gtest.h>
#include <../lib/UniquePtr.h>

TEST(UniquePtrTest, Constructor){
    int* ptr = new int(10);
    UniquePtr<int> uniquePtr(ptr);
    ASSERT_EQ(*ptr, 10);
}

TEST(UniquePtrTest, deleted_operator){
    auto ptr = UniquePtr<int>(new int(10));

}

TEST(UniquePtrTest, Assignment){

}