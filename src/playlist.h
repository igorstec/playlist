#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <utility>
#include <map>
#include <memory>
#include <list>
#include <stdexcept>

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
                const std::unique_ptr< P> params;
                TrackPositionsMap::iterator track_it;
            };

            // Shared data structure for CoW implementation
            struct PlaylistData {
                PlaylistItemList items;
                TrackPositionsMap track_positions_map;
            };

            std::shared_ptr<PlaylistData> data;

        // Ensure unique ownership of data before modification
        // If not unique, performs deep copy
        void ensure_unique() {
            if(data && data.use_count()>1) {
                //TODO: Deep copy implementation for this.data (unique_ptr problems for P)
            }
        }


        public:
            struct play_iterator {
                using iterator_concept = std::bidirectional_iterator_tag;
                using value_type       = PlaylistItem;
                using difference_type  = std::ptrdiff_t;

                using pointer          = const PlaylistItem*;
                using reference        = const PlaylistItem&;
            private:
                typename PlaylistItemList::const_iterator it;
            public:
                explicit play_iterator(typename PlaylistItemList::const_iterator iter) noexcept
                    : it(iter) {}
                play_iterator(const play_iterator& other) noexcept
                    : it(other.it) {}

                bool operator==(const play_iterator& other) const {
                    return it == other.it;
                }

                reference operator*() const {
                    return *it;
                };
                pointer operator->() const {
                    return &*it;
                };


                play_iterator& operator++() {
                    ++it;
                    return *this;
                };
                play_iterator& operator--() {
                    --it;
                    return *this;
                };

                play_iterator operator++(int) {
                    play_iterator temp = *this;
                    ++it;
                    return temp;

                };
                play_iterator operator--(int) {
                    play_iterator temp = *this;
                    --it;
                    return temp;
                };
            };

            struct sorted_iterator {
            public:
                using iterator_concept = std::bidirectional_iterator_tag;
                using value_type       = typename TrackPositionsMap::value_type;
                using difference_type  = std::ptrdiff_t;
                using pointer          = const value_type*;
                using reference        = const value_type&;

            private:
                typename TrackPositionsMap::const_iterator it;
            public:
                explicit sorted_iterator(typename TrackPositionsMap::const_iterator iter) noexcept
                    : it(iter) {}
                sorted_iterator(const sorted_iterator& other) noexcept
                    : it(other.it) {}

                bool operator==(const sorted_iterator& other) {
                    return it== other.it;
                }

                pointer operator->() const {
                    return &*it;
                };
                reference operator*() const {
                    return *it;
                }
                sorted_iterator& operator++() {
                    ++it;
                    return *this;
                };
                sorted_iterator operator++(int) {
                    sorted_iterator temp = *this;
                    ++it;
                    return temp;
                }

            };
        
            // Constructors
            playlist() : data(std::make_shared<PlaylistData>()) {}

            playlist(const playlist& that)
                : data(that.data) {
                // Copy-on-write: share data until modification
            }

            playlist(playlist&& that) noexcept
                : data(std::move(that.data)) {
                // Move constructor leaves original in consistent empty state which is safer
                that.data = std::make_shared<PlaylistData>();
            }

            ~playlist() noexcept = default;

            playlist& operator=(playlist that) noexcept {
                // Copy-and-swap idiom with argument by value
                // Exception safe: swap is noexcept and is faster than '='
                std::swap(data, that.data);
                return *this;
            }

            // Complexity: O(logn).
            void push_back(const T& track, const P& params) {
                ensure_unique();

                auto params_ptr = std::make_unique<P>(params);

                auto track_it = data->track_positions_map.find(track);
                std::shared_ptr<const T> track_ptr;

                if (track_it == data->track_positions_map.end()) {
                    track_ptr = std::make_shared<const T>(track);

                    auto emplaced = data->track_positions_map.emplace(
                        track_ptr, 
                        std::list<typename PlaylistItemList::const_iterator>{}
                    );
                    
                    track_it = emplaced.first;
                } else {
                    track_ptr = track_it->first;
                }

                data->items.emplace_back(
                    track_ptr, 
                    std::move(params_ptr), 
                    track_it
                );

                // Add the new track position to its position list.
                const auto item_it = --data->items.cend();
                track_it->second.push_back(item_it);
            }

            // Complexity: O(1) - even the map erasure is amortized constant.
            void pop_front() {
                if (data->items.empty()) {
                    throw std::out_of_range("Playlist is empty");
                }

                ensure_unique();

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
            
            // Complexity: O(1).
            const std::pair<const T&, const P&> front() const {
                if (data->items.empty()) {
                    throw std::out_of_range("Playlist is empty");
                }

                const auto& front_item = data->items.front();
                
                return {*front_item.track, *front_item.params};
            }

            // Complexity: O(logn + k),
            // where k is the number of occurences of the track in the playlist.
            void remove(const T& track) {
                const auto track_it = data->track_positions_map.find(track);
                if (track_it == data->track_positions_map.cend()) {
                    throw std::invalid_argument("Track is not on the playlist");
                }

                ensure_unique();

                const auto& positions = track_it->second;
                for (const auto position : positions) {
                    data->items.erase(position);
                }

                data->track_positions_map.erase(track_it);
            }

            void clear() {
                data->items.clear();
                data->track_positions_map.clear();
            };

            // Complexity: O(1).
            size_t size() {
                return data->items.size();
            }

            const std::pair<const T&, const P&> play(const play_iterator& it) const {
                return {*(it->track), *it->params};
            };

            const std::pair<const T&, size_t> pay(const sorted_iterator& it) const {
                return {*(it->first), it->second.size()};
            };

            P& params(const play_iterator& it) {
                ensure_unique();
                return *it->params;
            };

            const P& params(const play_iterator& it) const {
                return *it->params;
            };

            play_iterator play_begin() const {
                return play_iterator(data->items.begin());
            };

            play_iterator play_end() const {
                return play_iterator(data->items.end());
            };

            sorted_iterator sorted_begin() const {
                return sorted_iterator(data->track_positions_map.cbegin());
            };

            sorted_iterator sorted_end() const {
                return sorted_iterator(data->track_positions_map.cend());
            };
    };

} // namespace cxx

#endif