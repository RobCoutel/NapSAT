
#include "../src/utils/partition.hpp"

#include <catch2/catch.hpp>
#include <random>


TEST_CASE( "[AVL-Tree] Unit Test : Insertion in order" ) {
    utils::AVLTree tree;
    REQUIRE( tree.insert(1, nullptr) == true );
    REQUIRE( tree.insert(1, nullptr) == false );

    REQUIRE( tree.insert(2, nullptr) == true );
    REQUIRE( tree.insert(1, nullptr) == false );
    REQUIRE( tree.insert(2, nullptr) == false );

    REQUIRE( tree.insert(3, nullptr) == true );

    REQUIRE( tree.insert(1, nullptr) == false );
    REQUIRE( tree.insert(2, nullptr) == false );
    REQUIRE( tree.insert(3, nullptr) == false );
    REQUIRE(tree.is_balanced());
}

TEST_CASE( "[AVL-Tree] Unit Test : Insertion in reverse" ) {
    utils::AVLTree tree;
    REQUIRE( tree.insert(3, nullptr) == true );
    REQUIRE( tree.insert(2, nullptr) == true );
    REQUIRE( tree.insert(1, nullptr) == true );

    REQUIRE( tree.insert(1, nullptr) == false );
    REQUIRE( tree.insert(2, nullptr) == false );
    REQUIRE( tree.insert(3, nullptr) == false );
    REQUIRE(tree.is_balanced());
}

TEST_CASE( "[AVL-Tree] Unit Test : Insertion in random order" ) {
    utils::AVLTree tree;
    std::vector<unsigned> values;

    // seed the random
    std::seed_seq seed{0};
    std::mt19937 rd(seed);

    for (unsigned i = 0; i < 100; i++) {
        values.push_back(i);
    }

    std::shuffle(values.begin(), values.end(), rd);
    for (unsigned j = 0; j < 100; j++) {
        REQUIRE( tree.insert(values[j], nullptr) == true );
    }
    REQUIRE(tree.is_balanced());
}


TEST_CASE( "[AVL-Tree] Unit Test : Removal" ) {
    utils::AVLTree tree;
    tree.insert(1, nullptr);
    tree.insert(2, nullptr);
    tree.insert(3, nullptr);

    REQUIRE( tree.remove(1, nullptr) == true );
    REQUIRE( tree.remove(1, nullptr) == false );

    REQUIRE( tree.remove(2, nullptr) == true );
    REQUIRE( tree.remove(2, nullptr) == false );

    REQUIRE( tree.remove(3, nullptr) == true );
    REQUIRE( tree.remove(3, nullptr) == false );

    REQUIRE( tree.remove(1, nullptr) == false );
    REQUIRE( tree.remove(2, nullptr) == false );
}

TEST_CASE( "[AVL-Tree] Unit Test : Search" ) {
    utils::AVLTree tree;
    unsigned a = 1;
    unsigned b = 2;
    unsigned c = 3;

    tree.insert(1, &a);
    tree.insert(2, &b);
    tree.insert(3, &c);

    REQUIRE( tree.find_best_fit(0) == &a );
    REQUIRE( tree.find_best_fit(1) == &a );
    REQUIRE( tree.find_best_fit(2) == &b );
    REQUIRE( tree.find_best_fit(3) == &c );
    REQUIRE( tree.find_best_fit(4) == nullptr );
}
