﻿#ifndef _HS_SFM_SFM_UTILITY_MATCHES_TRACKS_CONVERTOR_HPP_
#define _HS_SFM_SFM_UTILITY_MATCHES_TRACKS_CONVERTOR_HPP_

#include <map>
#include <set>
#include <algorithm>
#include <queue>

#include "hs_sfm/sfm_utility/match_type.hpp"

namespace hs
{
namespace sfm
{

/**
 *  计算[匹配](@ref MatchContainer)与[tracks](@ref TrackContainer)的相互转换。
 */
struct MatchesTracksConvertor
{
  typedef int Err;

  struct KeyPairCompair
  {
    bool operator () (const KeyPair& key_pair0, const KeyPair& key_pair1) const
    {
      return key_pair0.first < key_pair1.first;
    }
  };

  Err operator() (const MatchContainer& matches,
                  TrackContainer& tracks) const
  {
#if 0
    typedef std::pair<size_t, size_t> View;
    typedef std::map<View, size_t> ViewTrackIndexer;
    typedef std::vector<std::pair<View, size_t> > ViewTrackContainer;
    tracks.clear();
    MatchContainer::const_iterator itr_image_pair = matches.begin();
    MatchContainer::const_iterator itr_image_pair_end = matches.end();
    ViewTrackIndexer view_track_indexer;
    size_t number_of_tracks = 0;
    ViewTrackContainer error_view_tracks;
    for (; itr_image_pair != itr_image_pair_end; ++itr_image_pair)
    {
      size_t image0_id = itr_image_pair->first.first;
      size_t image1_id = itr_image_pair->first.second;
      KeyPairContainer::const_iterator itr_key_pair =
        itr_image_pair->second.begin();
      KeyPairContainer::const_iterator itr_key_pair_end =
        itr_image_pair->second.end();
      for (; itr_key_pair != itr_key_pair_end; ++itr_key_pair)
      {
        size_t key0_id = itr_key_pair->first;
        size_t key1_id = itr_key_pair->second;
        View view0(image0_id, key0_id);
        View view1(image1_id, key1_id);
        ViewTrackIndexer::iterator itr_view0 = view_track_indexer.find(view0);
        ViewTrackIndexer::iterator itr_view1 = view_track_indexer.find(view1);
        if (itr_view0 != view_track_indexer.end())
        {
          if (itr_view1 != view_track_indexer.end())
          {
            if (itr_view0->second != itr_view1->second)
            {
              size_t track0_id = itr_view0->second;
              size_t track1_id = itr_view1->second;
              error_view_tracks.push_back(std::make_pair(view0, track0_id));
              error_view_tracks.push_back(std::make_pair(view1, track1_id));
            }
          }
          else
          {
            size_t track_id = itr_view0->second;
            view_track_indexer[view1] = track_id;
            tracks[track_id].push_back(view1);
          }
        }
        else
        {
          if (itr_view1 != view_track_indexer.end())
          {
            size_t track_id = itr_view1->second;
            view_track_indexer[view0] = track_id;
            tracks[track_id].push_back(view0);
          }
          else
          {
            view_track_indexer[view0] = number_of_tracks;
            view_track_indexer[view1] = number_of_tracks;
            number_of_tracks++;
            Track track;
            track.push_back(view0);
            track.push_back(view1);
            tracks.push_back(track);
          }
        }
      }
    }

    auto itr_error_view_track = error_view_tracks.begin();
    auto itr_error_view_track_end = error_view_tracks.end();
    std::cout<<"Error tracks!\n";
    for (; itr_error_view_track != itr_error_view_track_end;
         ++itr_error_view_track)
    {
      size_t track_id = itr_error_view_track->second;
      std::cout<<"error track "<<track_id<<"\n";
      for (size_t i = 0; i < tracks[track_id].size(); i++)
      {
        std::cout<<tracks[track_id][i].first<<" "
                 <<tracks[track_id][i].second<<"\n";
      }
      tracks[track_id].clear();
    }

    return 0;
#else
    typedef std::map<size_t, std::set<size_t> > ImageNeighborIndexer;
    typedef std::pair<size_t, size_t> View;

    //Normalize matches.Make it symmetric.Sort every key pairs in image pairs.
    MatchContainer matches_normalized = matches;
    auto itr_image_pair = matches.begin();
    auto itr_image_pair_end = matches.end();
    for (; itr_image_pair != itr_image_pair_end; ++itr_image_pair)
    {
      ImagePair image_pair = itr_image_pair->first;
      ImagePair image_pair_reverse = ImagePair(image_pair.second,
                                               image_pair.first);
      if (matches_normalized.find(image_pair_reverse) ==
          matches_normalized.end())
      {
        KeyPairContainer key_pairs_reverse;
        for (size_t i = 0; i < itr_image_pair->second.size(); i++)
        {
          const KeyPairContainer& key_pairs = itr_image_pair->second;
          KeyPair key_pair_reverse = KeyPair(key_pairs[i].second,
                                             key_pairs[i].first);
          key_pairs_reverse.push_back(key_pair_reverse);
        }
        matches_normalized[image_pair_reverse] = key_pairs_reverse;
      }
    }

    auto itr_image_pair_normalized = matches_normalized.begin();
    auto itr_image_pair_normalized_end = matches_normalized.end();
    for (; itr_image_pair_normalized != itr_image_pair_normalized_end;
         ++itr_image_pair_normalized)
    {
      std::sort(itr_image_pair_normalized->second.begin(),
                itr_image_pair_normalized->second.end());
    }

    //Build image pair indexer.
    ImageNeighborIndexer image_neighbor_indexer;
    itr_image_pair_normalized = matches_normalized.begin();
    itr_image_pair_normalized_end = matches_normalized.end();
    for (; itr_image_pair_normalized != itr_image_pair_normalized_end;
         ++itr_image_pair_normalized)
    {
      size_t image0_id = itr_image_pair_normalized->first.first;
      size_t image1_id = itr_image_pair_normalized->first.second;
      auto itr_image0_neighbor = image_neighbor_indexer.find(image0_id);
      if (itr_image0_neighbor == image_neighbor_indexer.end())
      {
        std::set<size_t> neighbors;
        neighbors.insert(image1_id);
        image_neighbor_indexer[image0_id] = neighbors;
      }
      else
      {
        itr_image0_neighbor->second.insert(image1_id);
      }
      auto itr_image1_neighbor = image_neighbor_indexer.find(image1_id);
      if (itr_image1_neighbor == image_neighbor_indexer.end())
      {
        std::set<size_t> neighbors;
        neighbors.insert(image0_id);
        image_neighbor_indexer[image1_id] = neighbors;
      }
      else
      {
        itr_image1_neighbor->second.insert(image0_id);
      }
    }

    itr_image_pair_normalized = matches_normalized.begin();
    itr_image_pair_normalized_end = matches_normalized.end();
    std::set<View> marked_views;
    std::set<View> error_views;
    hs::sfm::TrackContainer tracks_loose;
    for (; itr_image_pair_normalized != itr_image_pair_normalized_end;
         ++itr_image_pair_normalized)
    {
      auto itr_key_pair = itr_image_pair_normalized->second.begin();
      auto itr_key_pair_end = itr_image_pair_normalized->second.end();
      size_t image0_id = itr_image_pair_normalized->first.first;
      size_t image1_id = itr_image_pair_normalized->first.second;
      for (; itr_key_pair != itr_key_pair_end; ++itr_key_pair)
      {
        size_t key0_id = itr_key_pair->first;
        size_t key1_id = itr_key_pair->second;
        View view0(image0_id, key0_id);
        if (marked_views.find(view0) != marked_views.end())
        {
          continue;
        }
        marked_views.insert(view0);
        std::set<View> track_set;
        track_set.insert(view0);
        std::queue<View> view_queue;
        view_queue.push(view0);
        std::set<size_t> image_touched;
        image_touched.insert(image0_id);
        while (!view_queue.empty())
        {
          View view = view_queue.front();
          view_queue.pop();
          auto itr_neighbors = image_neighbor_indexer.find(view.first);
          if (itr_neighbors != image_neighbor_indexer.end())
          {
            auto itr_neighbor = itr_neighbors->second.begin();
            auto itr_neighbor_end = itr_neighbors->second.end();
            for (; itr_neighbor != itr_neighbor_end; ++itr_neighbor)
            {
              size_t image_id = *itr_neighbor;

              ImagePair image_pair(view.first, image_id);
              auto itr_image_pair =
                matches_normalized.find(ImagePair(view.first, image_id));
              if (itr_image_pair == matches_normalized.end())
              {
                return -1;
              }
              KeyPair key_pair_to_find(view.second, 0);
              auto search_result =
                std::equal_range(itr_image_pair->second.begin(),
                                 itr_image_pair->second.end(),
                                 key_pair_to_find, KeyPairCompair());
              if (search_result.first == search_result.second)
              {
                continue;
              }
              if (std::distance(search_result.first, search_result.second) > 1)
              {
                continue;
              }
              View view_new(image_id, search_result.first->second);
              if (track_set.find(view_new) != track_set.end())
              {
                continue;
              }

              //if (marked_views.find(view_new) != marked_views.end())
              //{
              //  continue;
              //}
              marked_views.insert(view_new);
              track_set.insert(view_new);
              view_queue.push(view_new);
              image_touched.insert(image_id);
            }// for (; itr_neighbor != itr_neighbor_end; ++itr_neighbor)
          }// if (itr_neighbors != image_neighbor_indexer.end())
        }// while (!view_queue.empty())
        if (track_set.size() > 1 && image_touched.size() == track_set.size())
        {
          Track track;
          auto itr_track = track_set.begin();
          auto itr_track_end = track_set.end();
          bool is_error = false;
          for (; itr_track != itr_track_end; ++itr_track)
          {
            if (error_views.find(*itr_track) != error_views.end())
            {
              is_error = true;
              break;
            }
            track.push_back(*itr_track);
          }

          if (!is_error)
          {
            tracks_loose.push_back(track);
          }
          else
          {
            itr_track = track_set.begin();
            itr_track_end = track_set.end();
            for (; itr_track != itr_track_end; ++itr_track)
            {
              error_views.insert(*itr_track);
            }
          }
        }
        else
        {
          auto itr_track = track_set.begin();
          auto itr_track_end = track_set.end();
          for (; itr_track != itr_track_end; ++itr_track)
          {
            error_views.insert(*itr_track);
          }
        }
      }// for (; itr_key_pair != itr_key_pair_end; ++itr_key_pair)
    } //for (; itr_image_pair_normalized != itr_image_pair_normalized_end;
      //     ++itr_image_pair_normalized)

    for (size_t i = 0; i < tracks_loose.size(); i++)
    {
      bool has_error = 0;
      for (size_t j = 0; j < tracks_loose[i].size(); j++)
      {
        if (error_views.find(tracks_loose[i][j]) != error_views.end())
        {
          has_error = true;
          break;
        }
      }
      if (has_error)
      {
        tracks_loose[i].clear();
      }
    }

    for (size_t i = 0; i < tracks_loose.size(); i++)
    {
      if (tracks_loose[i].size() > 1)
      {
        tracks.push_back(tracks_loose[i]);
      }
    }

    return 0;
#endif
  }

  Err operator() (const TrackContainer& tracks,
                  MatchContainer& matches) const
  {
    matches.clear();
    size_t number_of_tracks = tracks.size();
    for (size_t i = 0; i < number_of_tracks; i++)
    {
      size_t number_of_views = tracks[i].size();
      for (size_t j = 0; j < number_of_views - 1; j++)
      {
        size_t image0_id = tracks[i][j].first;
        size_t key0_id = tracks[i][j].second;
        size_t image1_id = tracks[i][j + 1].first;
        size_t key1_id = tracks[i][j + 1].second;
        matches[std::make_pair(image0_id, image1_id)].push_back(
          std::make_pair(key0_id, key1_id));
      }
    }
    return 0;
  }
};

}
}

#endif
