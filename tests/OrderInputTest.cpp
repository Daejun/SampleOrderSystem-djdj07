#include <gtest/gtest.h>

#include "controller/OrderInput.h"

TEST(OrderInputTest, ParsesPositiveInteger) {
    int quantity = 0;
    EXPECT_TRUE(orderinput::parsePositiveQuantity("42", quantity));
    EXPECT_EQ(quantity, 42);
}

TEST(OrderInputTest, RejectsZero) {
    int quantity = 0;
    EXPECT_FALSE(orderinput::parsePositiveQuantity("0", quantity));
}

TEST(OrderInputTest, RejectsNegative) {
    int quantity = 0;
    EXPECT_FALSE(orderinput::parsePositiveQuantity("-5", quantity));
}

TEST(OrderInputTest, RejectsNonNumeric) {
    int quantity = 0;
    EXPECT_FALSE(orderinput::parsePositiveQuantity("abc", quantity));
}

TEST(OrderInputTest, RejectsTrailingGarbage) {
    int quantity = 0;
    EXPECT_FALSE(orderinput::parsePositiveQuantity("5abc", quantity));
}
