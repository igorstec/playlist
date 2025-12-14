#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <utility>
#include <map>
#include <memory>
#include <list>
#include <stdexcept>
#include <iterator>

namespace cxx {

    template <typename T, typename P>
    class playlist {
        private:
            // Transparent comparator inspired by:
            // https://www.fluentcpp.com/2017/06/09/search-set-another-type-key/
            struct TrackPtrComparator {
                using is_transparent = void;

                bool operator()(
                    const std::shared_ptr<const T>& lhs,
                    const std::shared_ptr<const T>& rhs
                ) const {
                    return *lhs < *rhs;
                }

                bool operator()(
                    const std::shared_ptr<const T>& lhs,
                    const T& rhs
                ) const {
                    return *lhs < rhs;
                }

                bool operator()(
                    const T& lhs,
                    const std::shared_ptr<const T>& rhs
                ) const {
                    return lhs < *rhs;
                }
            };

            struct PlaylistItem;

            using PlaylistItemList = std::list<PlaylistItem>;

            using TrackPositionsMap = std::map<
                std::shared_ptr<const T>,
                std::list<typename PlaylistItemList::const_iterator>,
                TrackPtrComparator
            >;

            struct PlaylistItem {
                const std::shared_ptr<const T> track;
                const std::unique_ptr<P> params;
                TrackPositionsMap::iterator track_it;
            };

            // Shared data structure for Copy-on-Write implementation.
            struct PlaylistData {
                PlaylistItemList items;
                TrackPositionsMap track_positions_map;
            };

            std::shared_ptr<PlaylistData> data;

            std::shared_ptr<PlaylistData> clone_data() const {
                auto new_data = std::make_shared<PlaylistData>();

                for (const auto& item : data->items) {
                    auto new_params = std::make_unique<P>(*item.params);
                    new_data->items.emplace_back(
                        item.track,
                        std::move(new_params),
                        typename TrackPositionsMap::iterator()
                    );

                    auto track_it = new_data->track_positions_map.find(*item.track);
                    if (track_it == new_data->track_positions_map.end()) {
                        auto emplaced = new_data->track_positions_map.emplace(
                            item.track,
                            std::list<typename PlaylistItemList::const_iterator>()
                        );
                        track_it = emplaced.first;
                    }

                    // Add the new track position (current last added) to its position list.
                    const auto item_it = --new_data->items.end();
                    track_it->second.push_back(item_it);

                    // Update the item's track_it to point to the new map entry.
                    item_it->track_it = track_it;
                }

                return new_data;
            }

        public:
            struct play_iterator {
                friend class playlist;

                private:
                    typename PlaylistItemList::const_iterator it;

                public:
                    using iterator_concept = std::bidirectional_iterator_tag;
                    using difference_type  = std::ptrdiff_t;

                    explicit play_iterator(
                        typename PlaylistItemList::const_iterator iterator
                    ) noexcept
                    : it(iterator)
                    {}

                    play_iterator(const play_iterator& other) noexcept
                    : it(other.it)
                    {}

                    ~play_iterator(void) noexcept = default;

                    play_iterator& operator=(const play_iterator&) noexcept = default;

                    bool operator==(const play_iterator& other) const noexcept {
                        return it == other.it;
                    }

                    bool operator!=(const play_iterator& other) const noexcept {
                        return it != other.it;
                    }

                    play_iterator& operator++(void) noexcept {
                        ++it;
                        return *this;
                    }

                    play_iterator& operator--(void) noexcept {
                        --it;
                        return *this;
                    }

                    play_iterator operator++(int) noexcept {
                        play_iterator ret = *this;
                        ++it;
                        return ret;
                    }

                    play_iterator operator--(int) noexcept {
                        play_iterator ret = *this;
                        --it;
                        return ret;
                    }

            }; // play_iterator

            struct sorted_iterator {
                friend class playlist;

                private:
                    typename TrackPositionsMap::const_iterator it;

                public:
                    using iterator_concept = std::bidirectional_iterator_tag;
                    using difference_type  = std::ptrdiff_t;

                    explicit sorted_iterator(
                        typename TrackPositionsMap::const_iterator iterator
                    ) noexcept
                    : it(iterator)
                    {}

                    sorted_iterator(const sorted_iterator& other) noexcept
                    : it(other.it)
                    {}

                    ~sorted_iterator(void) noexcept = default;

                    sorted_iterator& operator=(const sorted_iterator&) noexcept = default;

                    bool operator==(const sorted_iterator& other) const noexcept {
                        return it == other.it;
                    }

                    bool operator!=(const sorted_iterator& other) const noexcept {
                        return it != other.it;
                    }

                    sorted_iterator& operator++(void) noexcept {
                        ++it;
                        return *this;
                    }

                    sorted_iterator operator++(int) noexcept {
                        sorted_iterator ret = *this;
                        ++it;
                        return ret;
                    }

            }; // sorted_iterator

            playlist() noexcept
            : data(std::make_shared<PlaylistData>())
            {}

            playlist(const playlist& that) noexcept
            : data(that.data)
            {}

            playlist(playlist&& that) noexcept
            : data(std::move(that.data))
            {
                that.data = std::make_shared<PlaylistData>();
            }

            ~playlist(void) noexcept = default;

            playlist& operator=(playlist that) noexcept {
                std::swap(data, that.data);
                return *this;
            }

            void push_back(const T& track, const P& params) {
                // Copy-on-Write: detach if shared.
                auto& target_data = data;
                bool should_copy = (data && data.use_count() > 1);

                if (should_copy) {
                    target_data = clone_data();
                }

                auto params_ptr = std::make_unique<P>(params);
                auto track_it = target_data->track_positions_map.find(track);
                bool inserted_in_map = false;

                if (track_it == target_data->track_positions_map.end()) {
                    auto track_ptr = std::make_shared<const T>(track);
                    auto emplaced = target_data->track_positions_map.emplace(
                        std::move(track_ptr),
                        std::list<typename PlaylistItemList::const_iterator>{}
                    );
                    track_it = emplaced.first;
                    inserted_in_map = true;
                }

                // Add to items list with strong exception safety.
                try {
                    target_data->items.emplace_back(
                        track_it->first,
                        std::move(params_ptr),
                        track_it
                    );
                } catch (...) {
                    if (inserted_in_map) {
                        target_data->track_positions_map.erase(track_it);
                    }
                    throw;
                }

                // Register item position in track's position list.
                try {
                    const auto item_it = --target_data->items.end();
                    track_it->second.push_back(item_it);
                } catch (...) {
                    target_data->items.pop_back();
                    if (inserted_in_map) {
                        target_data->track_positions_map.erase(track_it);
                    }
                    throw;
                }

                // Update shared data if it was cloned.
                if (should_copy) {
                    data = std::move(target_data);
                }
            }

            void pop_front() noexcept(false) {
                // Check if empty BEFORE COW check (strong exception safety).
                if (data->items.empty()) {
                    throw std::out_of_range("pop_front, playlist empty");
                }

                // Copy-on-Write: detach if shared.
                if (data && data.use_count() > 1) {
                    data = clone_data();
                }

                const auto& front_item = data->items.front();
                const auto track_it = front_item.track_it;

                // The positions in the list are naturally sorted,
                // so the earliest position is always at the front.
                auto& track_positions = track_it->second;
                track_positions.pop_front();

                if (track_positions.empty()) {
                    data->track_positions_map.erase(track_it);
                }

                data->items.pop_front();
            }

            const std::pair<const T&, const P&> front() const noexcept(false) {
                if (data->items.empty()) {
                    throw std::out_of_range("front, playlist empty");
                }

                const auto& front_item = data->items.front();

                return {*front_item.track, *front_item.params};
            }

            void remove(const T& track) noexcept(false) {
                // Check existence BEFORE COW check (strong exception safety).
                auto track_it = data->track_positions_map.find(track);
                if (track_it == data->track_positions_map.cend()) {
                    throw std::invalid_argument("remove, unknown track");
                }

                // Copy-on-Write: detach if shared.
                if (data && data.use_count() > 1) {
                    data = clone_data();
                    // Gets an iterator to the track's positions in the new data.
                    track_it = data->track_positions_map.find(track);
                }

                const auto& positions = track_it->second;
                for (const auto position : positions) {
                    data->items.erase(position);
                }

                data->track_positions_map.erase(track_it);
            }

            void clear() noexcept {
                data = std::make_shared<PlaylistData>();
            }

            size_t size() const noexcept {
                return data->items.size();
            }

            // Returns const reference to track and params at iterator position. O(1) complexity.
            const std::pair<const T&, const P&> play(const play_iterator& it) const noexcept {
                return {*(it.it->track), *(it.it->params)};
            }

            // Returns const reference to track and count of occurrences. O(k) where k is occurrence count.
            const std::pair<const T&, size_t> pay(const sorted_iterator& it) const noexcept {
                const auto& track_ptr = it.it->first;
                const auto& positions_list = it.it->second;
                
                return {*track_ptr, positions_list.size()};
            }

            P& params(const play_iterator& it) {
                auto iterator = it;

                // Copy-on-Write: detach if shared.
                if (data && data.use_count() > 1) {
                    auto pointed_index = std::distance(play_begin().it, it.it);
                    data = clone_data();
                    
                    auto items_it = data->items.begin();
                    std::advance(items_it, pointed_index);
                    iterator = play_iterator(items_it);
                }
                
                return *iterator.it->params;
            }

            // Const version: no COW needed, just access
            const P& params(const play_iterator& it) const {
                return *it.it->params;
            }

            play_iterator play_begin() const noexcept {
                return play_iterator(data->items.begin());
            }

            play_iterator play_end() const noexcept {
                return play_iterator(data->items.end());
            }

            sorted_iterator sorted_begin() const noexcept {
                return sorted_iterator(data->track_positions_map.cbegin());
            }

            sorted_iterator sorted_end() const noexcept {
                return sorted_iterator(data->track_positions_map.cend());
            }

    }; // playlist

} // namespace cxx

#endif