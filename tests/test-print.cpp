#include <catch2/catch.hpp>

#include "../src/utils/printer.hpp"

TEST_CASE ( "[Printer] Unit Test : Escaped length" ) {
  SECTION ("Empty") {
    REQUIRE(string_length_escaped("") == 0);
  }
  SECTION ("No escape characters") {
    REQUIRE(string_length_escaped("Hello, World!") == 13);
  }
  SECTION ("Single escape character") {
    REQUIRE(string_length_escaped("\033[0m") == 0);
  }
  SECTION ("Multiple escape characters") {
    REQUIRE(string_length_escaped("\033[0;31mHello, World!\033[0m") == 13);
  }
  SECTION ("Multiple escape characters with no text") {
    REQUIRE(string_length_escaped("\033[0;31m\033[0m") == 0);
  }
  SECTION ("Nested escape characters") {
    REQUIRE(string_length_escaped("\033[0;31mHello, \033[0m\033[0;32mWorld!\033[0m") == 13);
  }
}

TEST_CASE ( "[Printer] Unit Test : Padding" ) {
  SECTION ("Zero no padding") {
    REQUIRE(pad(0, 9) == "");
  }
  SECTION ("Zero padded once") {
    REQUIRE(pad(0, 10) == " ");
  }
  SECTION ("Zero padded twice") {
    REQUIRE(pad(0, 100) == "  ");
  }
  SECTION ("Single digit") {
    REQUIRE(pad(1, 9) == "");
  }
  SECTION ("Double digit no padding 1" ) {
    REQUIRE(pad(10, 99) == "");
  }
  SECTION ("Double digit no padding 2" ) {
    REQUIRE(pad(23, 36) == "");
  }
  SECTION ("Double digit padded once 1") {
    REQUIRE(pad(1, 99) == " ");
  }
  SECTION ("Double digit padded once 2") {
    REQUIRE(pad(1, 25) == " ");
  }
  SECTION ("Triple digit") {
    REQUIRE(pad(1, 999) == "  ");
  }
}
