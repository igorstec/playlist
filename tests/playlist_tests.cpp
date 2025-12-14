#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "../src/playlist.h"
#include <string>
#include <stdexcept>
#include <vector>
#include <utility>

using namespace cxx;

// Mock types for testing
struct Track {
    int id;
    std::string data;

    Track(int i, std::string d = "") : id(i), data(d) {}
    
    bool operator==(const Track& other) const { return id == other.id; }
    bool operator!=(const Track& other) const { return !(*this == other); }
    bool operator<(const Track& other) const { return id < other.id; }
    bool operator>(const Track& other) const { return id > other.id; }
    bool operator<=(const Track& other) const { return id <= other.id; }
    bool operator>=(const Track& other) const { return id >= other.id; }
};

struct Params {
    int volume;
    Params(int v = 0) : volume(v) {}
    bool operator==(const Params& other) const { return volume == other.volume; }
};

struct ThrowingParams {
    int val;
    static int copy_count;
    static int throw_at;
    
    ThrowingParams(int v) : val(v) {}
    ThrowingParams(const ThrowingParams& other) : val(other.val) {
        if (copy_count == throw_at) {
            throw std::runtime_error("Copy failed");
        }
        copy_count++;
    }
    bool operator==(const ThrowingParams& other) const { return val == other.val; }
};

int ThrowingParams::copy_count = 0;
int ThrowingParams::throw_at = -1;

TEST_SUITE("Playlist Specification Tests") {

    TEST_CASE("Constructors and Basic Accessors") {
        // Purpose: Test default constructor creates empty playlist.
        // Source: "Konstruktory: bezparametrowy tworzący pustą plejlistę... Złożoność O(1)."
        playlist<Track, Params> pl;
        CHECK(pl.size() == 0);
        
        // Purpose: Test size() returns 0 for empty playlist.
        // Source: "Metoda size zwraca liczbę utworów do odtworzenia na plejliście... Złożoność O(1)."
        CHECK(pl.size() == 0);

        // Purpose: Test pop_front throws on empty playlist.
        // Source: "Metoda pop_front ... Zgłasza wyjątek std::out_of_range, jeśli plejlista jest pusta."
        CHECK_THROWS_AS(pl.pop_front(), std::out_of_range);

        // Purpose: Test front throws on empty playlist.
        // Source: "Metoda front ... Zgłasza wyjątek std::out_of_range, jeśli plejlista jest pusta."
        CHECK_THROWS_AS(pl.front(), std::out_of_range);
    }

    TEST_CASE("Push Back and Front Access") {
        playlist<Track, Params> pl;
        Track t1(1, "Song1");
        Params p1(10);

        // Purpose: Test push_back inserts element.
        // Source: "Metoda push_back wstawia do odtworzenia na koniec listy utwór track z parametrami odtwarzania params."
        pl.push_back(t1, p1);
        CHECK(pl.size() == 1);

        // Purpose: Test front returns correct element.
        // Source: "Metoda front zwraca referencje na utwór i parametry jego odtwarzania z początku listy."
        auto front_elem = pl.front();
        CHECK(front_elem.first == t1);
        CHECK(front_elem.second == p1);

        Track t2(2, "Song2");
        Params p2(20);
        pl.push_back(t2, p2);
        CHECK(pl.size() == 2);
        
        // Front should still be t1
        CHECK(pl.front().first == t1);
    }

    TEST_CASE("Pop Front") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(2), Params(20));

        // Purpose: Test pop_front removes element from beginning.
        // Source: "Metoda pop_front usuwa z początku listy utwór i parametry jego odtwarzania."
        pl.pop_front();
        CHECK(pl.size() == 1);
        CHECK(pl.front().first == Track(2));

        pl.pop_front();
        CHECK(pl.size() == 0);
        CHECK_THROWS_AS(pl.pop_front(), std::out_of_range);
    }

    TEST_CASE("Remove") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(2), Params(20));
        pl.push_back(Track(1), Params(30));
        pl.push_back(Track(3), Params(40));

        // Purpose: Test remove removes all occurrences of a track.
        // Source: "Metoda remove usuwa z plejlisty wszystkie odtworzenia podanego utworu track."
        pl.remove(Track(1));
        CHECK(pl.size() == 2);
        
        // Remaining should be 2 and 3.
        // Order check via play_iterator would be better, but checking size and content via pop/front works too.
        CHECK(pl.front().first == Track(2));
        pl.pop_front();
        CHECK(pl.front().first == Track(3));

        // Purpose: Test remove throws if track not found.
        // Source: "Zgłasza wyjątek std::invalid_argument, jeśli nie ma takiego utworu na plejliście."
        CHECK_THROWS_AS(pl.remove(Track(99)), std::invalid_argument);
    }

    TEST_CASE("Clear") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(2), Params(20));

        // Purpose: Test clear empties the playlist.
        // Source: "Metoda clear opróżnia plejlistę... Złożoność O(n)."
        pl.clear();
        CHECK(pl.size() == 0);
        CHECK_THROWS_AS(pl.pop_front(), std::out_of_range);
    }

    TEST_CASE("Play Iterator") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(2), Params(20));
        pl.push_back(Track(1), Params(30));

        // Purpose: Test play_iterator iterates in insertion order.
        // Source: "Iterator play_iterator umożliwiający przeglądanie utworów w kolejności ich umieszczenia na plejliście (z powtórzeniami)."
        auto it = pl.play_begin();
        CHECK(it != pl.play_end());
        
        // Purpose: Test play() returns correct pair.
        // Source: "Metoda play zwraca referencje na utwór i parametry jego odtwarzania wskazywane przez podany iterator it."
        auto p1 = pl.play(it);
        CHECK(p1.first == Track(1));
        CHECK(p1.second == Params(10));

        ++it;
        auto p2 = pl.play(it);
        CHECK(p2.first == Track(2));
        CHECK(p2.second == Params(20));

        ++it;
        auto p3 = pl.play(it);
        CHECK(p3.first == Track(1));
        CHECK(p3.second == Params(30));

        ++it;
        CHECK(it == pl.play_end());
    }

    TEST_CASE("Sorted Iterator and Pay") {
        playlist<Track, Params> pl;
        pl.push_back(Track(2), Params(20));
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(1), Params(30));
        pl.push_back(Track(3), Params(40));

        // Purpose: Test sorted_iterator iterates in track order (unique).
        // Source: "Iterator sorted_iterator umożliwiający przeglądanie utworów na plejliście w porządku wyznaczonym przez typ T (bez powtórzeń)."
        auto it = pl.sorted_begin();
        CHECK(it != pl.sorted_end());

        // Should be Track(1)
        // Purpose: Test pay() returns track and count.
        // Source: "Metoda pay zwraca referencję na utwór wskazywany przez podany iterator it i liczbę jego wystąpień na plejliście."
        auto p1 = pl.pay(it);
        CHECK(p1.first == Track(1));
        CHECK(p1.second == 2); // Two occurrences of Track(1)

        ++it;
        // Should be Track(2)
        auto p2 = pl.pay(it);
        CHECK(p2.first == Track(2));
        CHECK(p2.second == 1);

        ++it;
        // Should be Track(3)
        auto p3 = pl.pay(it);
        CHECK(p3.first == Track(3));
        CHECK(p3.second == 1);

        ++it;
        CHECK(it == pl.sorted_end());
    }

    TEST_CASE("Params Access and Modification") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        auto it = pl.play_begin();

        // Purpose: Test params() gives access to parameters.
        // Source: "Metody params dają dostęp do parametrów utworu wskazywanego przez iterator it."
        CHECK(pl.params(it).volume == 10);

        // Modify params
        pl.params(it).volume = 99;
        CHECK(pl.params(it).volume == 99);
        
        // Check via play()
        CHECK(pl.play(it).second.volume == 99);
    }

    TEST_CASE("Copy on Write (CoW)") {
        // Purpose: Test Copy on Write semantics.
        // Source: "Kontener powinien realizować semantykę kopiowania przy modyfikowaniu (ang. copy on write)."
        
        playlist<Track, Params> pl1;
        pl1.push_back(Track(1), Params(10));
        
        // Copy constructor
        playlist<Track, Params> pl2 = pl1;
        
        // Check they share data (address of params should be same if CoW is implemented by sharing nodes)
        // Note: This assumes implementation detail, but CoW implies sharing.
        // If the implementation copies immediately, this check might fail, but spec says "współdzieli ona wszystkie wewnętrzne zasoby".
        auto it1 = pl1.play_begin();
        auto it2 = pl2.play_begin();
        
        // We can't easily check internal pointers without violating encapsulation, 
        // but we can check if modifying one affects the other (it shouldn't) 
        // and if modifying one changes the address of its elements (it should detach).
        
        [[maybe_unused]] const Params* addr1 = &pl1.params(it1);
        [[maybe_unused]] const Params* addr2 = &pl2.params(it2);
        
        // Ideally addr1 == addr2 here if fully shared, but let's focus on behavior.
        
        // Modify pl2
        pl2.push_back(Track(2), Params(20));
        
        // Now pl2 should have detached. pl1 should be unchanged.
        CHECK(pl1.size() == 1);
        CHECK(pl2.size() == 2);
        
        // Check if pl1 is still intact
        CHECK(pl1.play(pl1.play_begin()).first == Track(1));
        
        // Check if modification of pl2 params doesn't affect pl1
        auto it2_new = pl2.play_begin(); // Iterator to first element of pl2
        pl2.params(it2_new).volume = 999;
        
        CHECK(pl1.params(pl1.play_begin()).volume == 10); // Should still be 10
        CHECK(pl2.params(it2_new).volume == 999);
    }

    TEST_CASE("Exception Safety - Strong Guarantee") {
        // Purpose: Test strong exception guarantee.
        // Source: "Wszystkie operacje muszą zapewniać co najmniej silną gwarancję odporności na wyjątki"
        
        // Reset statics
        ThrowingParams::copy_count = 0;
        ThrowingParams::throw_at = -1; // Don't throw yet

        playlist<Track, ThrowingParams> pl;
        pl.push_back(Track(1), ThrowingParams(10));
        pl.push_back(Track(2), ThrowingParams(20));
        
        // State before operation
        size_t size_before = pl.size();
        
        // Set to throw on next copy
        ThrowingParams::copy_count = 0;
        ThrowingParams::throw_at = 0; // Throw immediately on copy
        
        // Try to push back - should fail and leave playlist unchanged
        // Note: push_back takes T const& and P const&. It will copy P.
        CHECK_THROWS_AS(pl.push_back(Track(3), ThrowingParams(30)), std::runtime_error);
        
        // Verify state is unchanged
        CHECK(pl.size() == size_before);
        CHECK(pl.front().first == Track(1));
    }

    TEST_CASE("Exception Safety - Remove") {
        // Purpose: Test strong exception guarantee during remove (which triggers CoW).
        
        ThrowingParams::copy_count = 0;
        ThrowingParams::throw_at = -1;

        playlist<Track, ThrowingParams> pl1;
        pl1.push_back(Track(1), ThrowingParams(10));
        pl1.push_back(Track(2), ThrowingParams(20));
        
        playlist<Track, ThrowingParams> pl2 = pl1; // Share
        
        // Now pl2 shares with pl1.
        // Calling remove on pl2 should trigger CoW.
        // If CoW throws, pl2 should remain equal to pl1 (shared).
        
        ThrowingParams::copy_count = 0;
        ThrowingParams::throw_at = 0; // Throw on first copy during CoW
        
        CHECK_THROWS_AS(pl2.remove(Track(1)), std::runtime_error);
        
        // pl2 should still have 2 elements and be equal to pl1
        CHECK(pl2.size() == 2);
        CHECK(pl2.front().first == Track(1));
    }
    
    TEST_CASE("Iterator Invalidation Rules") {
        // Purpose: Test iterator invalidation on modification.
        // Source: Forum Q4: "Jeśli metoda params wykonała kopię, to iteratory ulegają unieważnieniu..."
        // We can't easily test "invalidation" (segfault), but we can verify that iterators from before a CoW detach 
        // might point to the old data (which is fine if the old data is kept alive by the other copy) 
        // or the new data if not.
        // Actually, the spec says: "W szczególności operacje modyfikujące zakończone niepowodzeniem nie mogą unieważniać iteratorów."
        // And Forum Q4 says params (non-const) might invalidate if it causes a copy.
        
        playlist<Track, Params> pl1;
        pl1.push_back(Track(1), Params(10));
        
        playlist<Track, Params> pl2 = pl1; // Share
        
        auto it1 = pl1.play_begin();
        
        // Modify pl1 via params (non-const) - this should trigger CoW (detach)
        pl1.params(it1).volume = 20;
        
        // Refresh it1 because the previous it1 is invalidated by CoW
        auto it1_new = pl1.play_begin();

        // Now pl1 and pl2 should be separate.
        CHECK(pl2.params(pl2.play_begin()).volume == 10);
        CHECK(pl1.params(it1_new).volume == 20);
        
        // The iterator it1 was used to call params. If params triggered CoW, it1 might be technically invalidated 
        // in the sense that it points to the *new* structure? Or does params return reference to the element 
        // and *then* we modify it?
        // `pl1.params(it1)` returns `P&`. Inside `params`, if CoW is needed, `pl1` creates a new copy of data.
        // If `it1` was pointing to the old data (shared with `pl2`), and `pl1` now has new data, 
        // `params` must ensure it returns a reference to the *new* data corresponding to `it1`.
        // But `it1` itself is passed by const reference. It can't be updated by `params`.
        // So `params` must find the element corresponding to `it1` in the new structure?
        // Or `it1` is just an index/offset?
        // If `it1` is a pointer, it points to old data.
        // If `params` detaches, it creates new data.
        // If `params` returns reference to new data, it must map `it1` to new data.
        // This suggests iterators might be indices or the implementation handles this mapping.
        // However, the forum says: "Jeśli metoda params wykonała kopię, to iteratory ulegają unieważnieniu... w zasadzie nie ma powodu, aby iteratory przestały wskazywać na obiekty, na które dotąd wskazywały, ale nie bardzo wiadomo, na którą z kopii wskazują."
        // This implies using `it1` (which points to old shared data) after `pl1` has detached might be risky or undefined if we expect it to point to `pl1`'s data.
        // But here we used `it1` *to call* `params`.
        // `pl1.params(it1)` -> triggers detach -> returns ref to new data.
        // The `it1` passed in still points to old data (shared with `pl2`).
        // If the implementation supports this, good.
    }

    TEST_CASE("Iterator Operators") {
        playlist<Track, Params> pl;
        pl.push_back(Track(1), Params(10));
        pl.push_back(Track(2), Params(20));
        
        auto it = pl.play_begin();
        auto it2 = it; // Copy/Assignment
        CHECK(it == it2);
        
        auto it3 = ++it; // Pre-increment
        CHECK(it == it3);
        CHECK(it != it2);
        
        auto it4 = it++; // Post-increment
        CHECK(it4 == it3);
        CHECK(it != it4);
        CHECK(it == pl.play_end());
    }
}
