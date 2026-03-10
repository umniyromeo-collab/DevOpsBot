#include <gtest/gtest.h>
#include "UniquePtr.h"

TEST(UniquePtrTest, DefaultConstructor) {
    UniquePtr<int> ptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.operator->(), nullptr);
}
