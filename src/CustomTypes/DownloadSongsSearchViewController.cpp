#include "CustomTypes/DownloadSongsSearchViewController.hpp"

#include "CustomLogger.hpp"
#include "CustomTypes/DownloadSongsPlaylistViewController.hpp"
#include "ModConfig.hpp"

#include "custom-types/shared/delegate.hpp"

#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/SpriteMeshType.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Events/UnityAction.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"
#include "HMUI/ScrollView.hpp"
#include "HMUI/Touchable.hpp"
#include "System/Action.hpp"

#include "GlobalNamespace/LevelBar.hpp"

#include "bsml/shared/BSML/Components/ExternalComponents.hpp"
#include "bsml/shared/BSML/Components/Backgroundable.hpp"
#include "bsml/shared/BSML.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace UnityEngine;
using namespace UnityEngine::UI;
using namespace UnityEngine::Events;
using namespace HMUI;
using namespace TMPro;
using namespace GlobalNamespace;
using namespace BSML::Lite;

using namespace SongDownloader;

namespace SongDownloader {
    DownloadSongsSearchViewController* searchViewController;
}

DEFINE_TYPE(SongDownloader, DownloadSongsSearchViewController);

int DownloadSongsSearchViewController::searchIndex = 0;

int DownloadSongsSearchViewController::searchPage = 0;

std::string DownloadSongsSearchViewController::SearchQuery = "";

void DownloadSongsSearchViewController::CreateEntries(Transform* parent) {
    static ConstString songArtworkName("SongArtwork");
    static GameObject* prefab = nullptr;
    if(!prefab) {
        GameObject* holder = GameObject::New_ctor();
        Object::DontDestroyOnLoad(holder);
        auto levelBarLayout = CreateHorizontalLayoutGroup(holder->get_transform());
        prefab = levelBarLayout->get_gameObject();
        levelBarLayout->set_childControlWidth(false);
        levelBarLayout->set_childForceExpandWidth(true);

        LayoutElement* levelBarLayoutElement = levelBarLayout->GetComponent<LayoutElement*>();
        levelBarLayoutElement->set_minHeight(15.0f);
        levelBarLayoutElement->set_minWidth(90.0f);

        GameObject* existingLevelBar = Resources::FindObjectsOfTypeAll<LevelBar*>()->First(
            [](LevelBar* x) {
                return x->get_name() == "LevelBarBig";
            })->get_gameObject();
        GameObject* levelBarGameObject = UnityEngine::GameObject::Instantiate(existingLevelBar, levelBarLayout->get_transform());
        auto levelBarTransform = levelBarGameObject->get_transform();

        static ConstString multipleLineTextContainerName("MultipleLineTextContainer");
        Object::Destroy(levelBarTransform->FindChild(multipleLineTextContainerName)->get_gameObject());
        static ConstString favoriteToggleName("FavoriteToggle");
        Object::Destroy(levelBarTransform->FindChild(favoriteToggleName)->get_gameObject()); 
        static ConstString singleLineTextContainerName("SingleLineTextContainer");
        levelBarTransform->FindChild(singleLineTextContainerName)->get_gameObject()->set_active(true);
        LevelBar* levelBar = levelBarGameObject->GetComponent<LevelBar*>();
        auto songNameTextComponent = levelBar->_songNameText;
        songNameTextComponent->set_fontSize(4.2f);
        songNameTextComponent->set_overflowMode(TextOverflowModes::Ellipsis);
        songNameTextComponent->set_margin(Vector4(-2.0f, 0.0f, 9.0f, 0.0f));

        auto authorNameTextComponent = levelBar->_authorNameText;
        authorNameTextComponent->set_richText(true);
        authorNameTextComponent->set_fontSize(3.2f);
        authorNameTextComponent->set_overflowMode(TextOverflowModes::Ellipsis);
        authorNameTextComponent->set_margin(Vector4(-2.0f, 0.0f, 9.0f, 0.0f));

        static ConstString bgName("BG");
        Transform* backgroundTransform = levelBarTransform->Find(bgName);
        backgroundTransform->set_localScale(Vector3(1.5f, 1.0f, 1.0f));
        levelBar->get_transform()->Find(songArtworkName)->set_localScale(Vector3(0.96f, 0.96f, 0.96f));

        Button* prefabDownloadButton = CreateUIButton(levelBarTransform, "Download", "PracticeButton", {0, 0}, {-5, 5}, nullptr);
        prefab->SetActive(false);
    }
    for(int i = 0; i < ENTRIES_PER_PAGE; i++) {
        auto copy = Object::Instantiate(prefab, parent);
        LevelBar* copyLevelBar = copy->GetComponentInChildren<LevelBar*>();
        Button* downloadButton = copy->GetComponentInChildren<Button*>();
        downloadButton->get_transform()->set_localPosition(Vector3(36.5f, -7.0f, 0.0f));
        Transform* artworkTransform = copyLevelBar->get_transform()->Find(songArtworkName);
        HMUI::ImageView* artwork = artworkTransform->GetComponent<HMUI::ImageView*>();

        searchEntries[i] = SearchEntry(copy, copyLevelBar->_songNameText, copyLevelBar->_authorNameText, artwork, downloadButton);
        auto& entry = searchEntries[i];
        downloadButton->get_onClick()->AddListener(custom_types::MakeDelegate<UnityAction*>(
            (std::function<void()>) [this, &entry] {
                if (entry.MapType == SearchEntry::MapType::BeatSaver) {
                    auto hash = entry.GetBeatmap().GetVersions().front().GetHash();
                    BeatSaver::API::DownloadBeatmapAsync(entry.GetBeatmap(),
                        [this, hash](bool error) {
                            if (!error) {
                                if (auto playlist = DownloadSongsPlaylistViewController::GetSelectedPlaylist()) {
                                    auto& json = playlist->playlistJSON;
                                    json.Songs.emplace_back().Hash = hash;
                                    playlist->Save();
                                    PlaylistCore::MarkPlaylistForReload(playlist);
                                }
                                SongCore::API::Loading::RefreshSongs(false);
                            }
                        },
                        [&entry, hash](float percentage) {
                            if (entry.GetBeatmap().GetVersions().front().GetHash() == hash) {
                                entry.downloadProgress = percentage;
                                BSML::MainThreadScheduler::Schedule(
                                    [&entry] {
                                        entry.UpdateDownloadProgress(false);
                                    }
                                );
                            }
                        }
                    );
                }
                else if (entry.MapType == SearchEntry::MapType::BeastSaber) {
                    auto hash = entry.GetSongBeastSaber().GetHash();
                    BeatSaver::API::DownloadBeatmapAsync(entry.GetSongBeastSaber(),
                        [this, hash](bool error) {
                            if (!error) {
                                if (auto playlist = DownloadSongsPlaylistViewController::GetSelectedPlaylist()) {
                                    auto& json = playlist->playlistJSON;
                                    json.Songs.emplace_back().Hash = hash;
                                    playlist->Save();
                                    PlaylistCore::MarkPlaylistForReload(playlist);
                                }
                                SongCore::API::Loading::RefreshSongs(false);
                            }
                        },
                        [&entry, hash](float percentage) {
                            if (entry.GetSongBeastSaber().GetHash() == hash) {
                                entry.downloadProgress = percentage;
                                BSML::MainThreadScheduler::Schedule(
                                    [&entry] {
                                        entry.UpdateDownloadProgress(false);
                                    }
                                );
                            }
                        }
                    );
                }
                else {
                    auto hash = entry.GetSongScoreSaber().GetId();
                    BeatSaver::API::DownloadBeatmapAsync(entry.GetSongScoreSaber(),
                        [this, hash](bool error) {
                            if (!error) {
                                if (auto playlist = DownloadSongsPlaylistViewController::GetSelectedPlaylist()) {
                                    auto& json = playlist->playlistJSON;
                                    json.Songs.emplace_back().Hash = hash;
                                    playlist->Save();
                                    PlaylistCore::MarkPlaylistForReload(playlist);
                                }
                                SongCore::API::Loading::RefreshSongs(false);
                            }
                        },
                        [&entry, hash](float percentage) {
                            if (entry.GetSongScoreSaber().GetId() == hash) {
                                entry.downloadProgress = percentage;
                                BSML::MainThreadScheduler::Schedule(
                                    [&entry] {
                                        entry.UpdateDownloadProgress(false);
                                    }
                                );
                            }
                        }
                    );
                }
            }
        ));
    }
}

std::optional<bool> StringToBool(std::string value) {
    if (value == "true") return true;
    else if (value == "false") return false;
    else return std::nullopt;
}

#pragma region SearchType Functions

void DownloadSongsSearchViewController::SearchKey(int currentSearchIndex) {
    if (!DownloadSongsSearchViewController::SearchQuery.empty()) {
        BeatSaver::API::GetBeatmapByKeyAsync(DownloadSongsSearchViewController::SearchQuery,
            [this, currentSearchIndex](std::optional<BeatSaver::Beatmap> beatmap) {
                if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                    BSML::MainThreadScheduler::Schedule(
                        [this, currentSearchIndex, beatmap] {
                            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                                if (beatmap.has_value()) {
                                    loadingControl->Hide();
                                    searchEntries[0].SetBeatmap(beatmap.value());
                                    for (int i = 1; i < ENTRIES_PER_PAGE; i++) {
                                        searchViewController->searchEntries[i].Disable();
                                    }
                                }
                                else {
                                    if (!BeatSaver::API::exception.empty()) loadingControl->ShowText(BeatSaver::API::exception, true);
                                    else loadingControl->ShowText("No Song Found for Key!", true);
                                }
                            }
                        }
                    );
                }
            }
        );
    }
    else loadingControl->ShowText("Please type in a key!", false);
}

void DownloadSongsSearchViewController::SearchSongs(int currentSearchIndex) {
    BeatSaver::API::SearchPagedAsync(DownloadSongsSearchViewController::SearchQuery, DownloadSongsSearchViewController::searchPage,
        [this, currentSearchIndex](std::optional<BeatSaver::Page> page) {
            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, page] {
                        if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                            if (page.has_value() && !page.value().GetDocs().empty()) {
                                auto maps = page.value().GetDocs();
                                auto mapsSize = maps.size();
                                int mapIndex = 0;
                                for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                    auto& searchEntry = searchEntries[i];
                                    if (mapIndex < mapsSize) {
                                        loadingControl->Hide();
                                        auto& map = maps.at(mapIndex);
                                        searchEntry.SetBeatmap(map);
                                    }
                                    else {
                                        searchEntry.Disable();
                                    }
                                    mapIndex++;
                                }
                            }
                            else {
                                if (!BeatSaver::API::exception.empty()) loadingControl->ShowText(BeatSaver::API::exception, true);
                                else if (DownloadSongsSearchViewController::SearchQuery.empty()) loadingControl->ShowText("No Results,\nis your Internet working?", true);
                                else loadingControl->ShowText("No Songs Found!", true);
                            }
                        }
                    }
                );
            }
        },
        getModConfig().SortOrder.GetValue(),
            getModConfig().AutoMapper.GetValue(),
            getModConfig().Ranked.GetValue(),
            getModConfig().ME.GetValue(),
            getModConfig().NE.GetValue(),
            getModConfig().Chroma.GetValue());
}

void DownloadSongsSearchViewController::SearchUser(int currentSearchIndex) {
    if (!DownloadSongsSearchViewController::SearchQuery.empty()) {
        BeatSaver::API::GetUserByNameAsync(DownloadSongsSearchViewController::SearchQuery,
            [this, currentSearchIndex](std::optional<BeatSaver::UserDetail> User) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, User]() {
                        if (User.has_value()) {
                            BeatSaver::API::GetBeatmapByUserIdAsync(User.value().GetId(), DownloadSongsSearchViewController::searchPage,
                                [this, currentSearchIndex](std::optional<BeatSaver::Page> page) {
                                    BSML::MainThreadScheduler::Schedule(
                                        [this, currentSearchIndex, page]() {
                                            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                                                if (page.has_value() && !page.value().GetDocs().empty()) {
                                                    auto maps = page.value().GetDocs();
                                                    auto mapsSize = maps.size();
                                                    int mapIndex = 0;
                                                    for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                                        auto& searchEntry = searchEntries[i];
                                                        if (mapIndex < mapsSize) {
                                                            loadingControl->Hide();
                                                            auto& map = maps.at(mapIndex);
                                                            searchEntry.SetBeatmap(map);
                                                        }
                                                        else {
                                                            searchEntry.Disable();
                                                        }
                                                        mapIndex++;
                                                    }
                                                } else {
                                                    if (!BeatSaver::API::exception.empty() && BeatSaver::API::exception != "Not Found") loadingControl->ShowText(BeatSaver::API::exception, true);
                                                    else loadingControl->ShowText("No Songs Found for given User!", true);
                                                }
                                            }
                                        }
                                    );
                                }
                            );
                        } else {
                            if (!BeatSaver::API::exception.empty() && BeatSaver::API::exception != "Not Found") loadingControl->ShowText(BeatSaver::API::exception, true);
                            else loadingControl->ShowText("No User Found!", true);
                        }
                    }
                );
            }
        );
    }
    else loadingControl->ShowText("Please type in a Username!", false);
}

void DownloadSongsSearchViewController::GetCuratorRecommended(int currentSearchIndex) {
        BeastSaber::API::CuratorRecommendedAsync(
            [this, currentSearchIndex](std::optional<BeastSaber::Page> page) {
                if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                    BSML::MainThreadScheduler::Schedule(
                        [this, currentSearchIndex, page] {
                            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                                if (page.has_value()) {
                                    auto songs = page.value().GetSongs();
                                    auto songsSize = songs.size();
                                    int songIndex = 0;
                                    for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                        auto& searchEntry = searchEntries[i];
                                        if (songIndex < songsSize) {
                                            loadingControl->Hide();
                                            auto& song = songs.at(songIndex);
                                            searchEntry.SetBeatmap(song);
                                        }
                                        else {
                                            searchEntry.Disable();
                                        }
                                        songIndex++;
                                    }
                                }
                                else {
                                    if (!BeastSaber::API::exception.empty()) loadingControl->ShowText(BeastSaber::API::exception, true);
                                    else loadingControl->ShowText("Damn, Curators didn't recommend anything!", true);
                                }
                            }
                        }
                    );
                }
            }
        , DownloadSongsSearchViewController::searchPage);
}

void DownloadSongsSearchViewController::GetBookmarks(int currentSearchIndex) {
    if (!DownloadSongsSearchViewController::SearchQuery.empty()) {
        BeastSaber::API::BookmarkedAsync(DownloadSongsSearchViewController::SearchQuery,
            [this, currentSearchIndex](std::optional<BeastSaber::Page> page) {
                if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                    BSML::MainThreadScheduler::Schedule(
                        [this, currentSearchIndex, page] {
                            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                                if (page.has_value() && !page.value().GetSongs().empty()) {
                                    auto songs = page.value().GetSongs();
                                    auto songsSize = songs.size();
                                    int songIndex = 0;
                                    for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                        auto& searchEntry = searchEntries[i];
                                        if (songIndex < songsSize) {
                                            loadingControl->Hide();
                                            auto& song = songs.at(songIndex);
                                            searchEntry.SetBeatmap(song);
                                        }
                                        else {
                                            searchEntry.Disable();
                                        }
                                        songIndex++;
                                    }
                                }
                                else {
                                    if (!BeastSaber::API::exception.empty()) loadingControl->ShowText(BeastSaber::API::exception, true);
                                    else loadingControl->ShowText("No bookmarks found\nfor given User!", true);
                                }
                            }
                        }
                    );
                }
            },
        DownloadSongsSearchViewController::searchPage);
    }
    else loadingControl->ShowText("Please type in a Username!", false);
}

void DownloadSongsSearchViewController::GetTrending(int currentSearchIndex) {
    ScoreSaber::API::SearchAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::ListCategory::TopRanked,
    //ScoreSaber::API::GetTrendingAsync(
        [this, currentSearchIndex](std::optional<ScoreSaber::Leaderboards> page) {
            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, page] {
                        if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                            if (page.has_value() && !page.value().GetLeaderboards().empty()) {
                                auto songs = page.value().GetLeaderboards();
                                auto songsSize = songs.size();
                                int songIndex = 0;
                                for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                    auto& searchEntry = searchEntries[i];
                                    if (songIndex < songsSize) {
                                        loadingControl->Hide();
                                        auto& song = songs.at(songIndex);
                                        searchEntry.SetBeatmap(song);
                                    }
                                    else {
                                        searchEntry.Disable();
                                    }
                                    songIndex++;
                                }
                            }
                            else {
                                if (!ScoreSaber::API::exception.empty()) loadingControl->ShowText(ScoreSaber::API::exception, true);
                                else if (DownloadSongsSearchViewController::SearchQuery.empty()) loadingControl->ShowText("No Results,\nis your Internet working?", true);
                                else loadingControl->ShowText("No Songs Found!", true);
                            }
                        }
                    }
                );
            }
        },
        true, std::nullopt, true, DownloadSongsSearchViewController::searchPage); // TODO: Possibly add option to search by qualified, for now just nullopt on that parameter
}

void DownloadSongsSearchViewController::GetLatestRanked(int currentSearchIndex) {
    ScoreSaber::API::SearchAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::ListCategory::LatestRanked,
    //ScoreSaber::API::SearchSSAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::SearchType::LatestRanked,
        [this, currentSearchIndex](std::optional<ScoreSaber::Leaderboards> page) {
            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, page] {
                        if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                            if (page.has_value() && !page.value().GetLeaderboards().empty()) {
                                auto songs = page.value().GetLeaderboards();
                                auto songsSize = songs.size();
                                int songIndex = 0;
                                for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                    auto& searchEntry = searchEntries[i];
                                    if (songIndex < songsSize) {
                                        loadingControl->Hide();
                                        auto& song = songs.at(songIndex);
                                        searchEntry.SetBeatmap(song);
                                    }
                                    else {
                                        searchEntry.Disable();
                                    }
                                    songIndex++;
                                }
                            }
                            else {
                                if (!ScoreSaber::API::exception.empty()) loadingControl->ShowText(ScoreSaber::API::exception, true);
                                else if (DownloadSongsSearchViewController::SearchQuery.empty()) loadingControl->ShowText("No Results,\nis your Internet working?", true);
                                else loadingControl->ShowText("No Songs Found!", true);
                            }
                        }
                    }
                );
            }
        },
        std::nullopt, std::nullopt, true, DownloadSongsSearchViewController::searchPage); // TODO: Possibly add option to search by qualified, for now just nullopt on that parameter
}

void DownloadSongsSearchViewController::GetTopPlayed(int currentSearchIndex) {
    ScoreSaber::API::GetListAsync(ScoreSaber::API::ListCategory::TopPlayed, /*)
    ScoreSaber::API::SearchSSAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::SearchType::TopPlayed,*/
        [this, currentSearchIndex](std::optional<ScoreSaber::Leaderboards> page) {
            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, page] {
                        if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                            if (page.has_value() && !page.value().GetLeaderboards().empty()) {
                                auto songs = page.value().GetLeaderboards();
                                auto songsSize = songs.size();
                                int songIndex = 0;
                                for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                    auto& searchEntry = searchEntries[i];
                                    if (songIndex < songsSize) {
                                        loadingControl->Hide();
                                        auto& song = songs.at(songIndex);
                                        searchEntry.SetBeatmap(song);
                                    }
                                    else {
                                        searchEntry.Disable();
                                    }
                                    songIndex++;
                                }
                            }
                            else {
                                if (!ScoreSaber::API::exception.empty()) loadingControl->ShowText(ScoreSaber::API::exception, true);
                                else if (DownloadSongsSearchViewController::SearchQuery.empty()) loadingControl->ShowText("No Results,\nis your Internet working?", true);
                                else loadingControl->ShowText("No Songs Found!", true);
                            }
                        }
                    }
                );
            }
        },
        StringToBool(getModConfig().Ranked.GetValue()), std::nullopt, true, DownloadSongsSearchViewController::searchPage); // TODO: Possibly add option to search by qualified, for now just nullopt on that parameter
}

void DownloadSongsSearchViewController::GetTopRanked(int currentSearchIndex) {
    ScoreSaber::API::SearchAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::ListCategory::TopRanked,
    //ScoreSaber::API::SearchSSAsync(DownloadSongsSearchViewController::SearchQuery, ScoreSaber::API::SearchType::TopRanked,
        [this, currentSearchIndex](std::optional<ScoreSaber::Leaderboards> page) {
            if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                BSML::MainThreadScheduler::Schedule(
                    [this, currentSearchIndex, page] {
                        if (currentSearchIndex == DownloadSongsSearchViewController::searchIndex) {
                            if (page.has_value() && !page.value().GetLeaderboards().empty()) {
                                auto songs = page.value().GetLeaderboards();
                                auto songsSize = songs.size();
                                int songIndex = 0;
                                for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
                                    auto& searchEntry = searchEntries[i];
                                    if (songIndex < songsSize) {
                                        loadingControl->Hide();
                                        auto& song = songs.at(songIndex);
                                        searchEntry.SetBeatmap(song);
                                    }
                                    else {
                                        searchEntry.Disable();
                                    }
                                    songIndex++;
                                }
                            }
                            else {
                                if (!ScoreSaber::API::exception.empty()) loadingControl->ShowText(ScoreSaber::API::exception, true);
                                else if (DownloadSongsSearchViewController::SearchQuery.empty()) loadingControl->ShowText("No Results,\nis your Internet working?", true);
                                else loadingControl->ShowText("No Songs Found!", true);
                            }
                        }
                    }
                );
            }
        },
        std::nullopt, std::nullopt, true, DownloadSongsSearchViewController::searchPage); // TODO: Possibly add option to search by qualified, for now just nullopt on that parameter
}
#pragma endregion

void DownloadSongsSearchViewController::Search() {
    for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
        searchViewController->searchEntries[i].Disable();
    }
    searchViewController->loadingControl->ShowLoading("Loading...");
    DownloadSongsSearchViewController::searchIndex++;
    int currentSearchIndex = DownloadSongsSearchViewController::searchIndex;
    searchViewController->SearchField->get_gameObject()->SetActive(true);
    if (getModConfig().Service.GetValue() == "BeatSaver") {
        if (getModConfig().ListType_BeatSaver.GetValue() == "Key") {
            searchViewController->SearchKey(currentSearchIndex);
        }
        else if (getModConfig().ListType_BeatSaver.GetValue() == "Search") {
            searchViewController->SearchSongs(currentSearchIndex);
        }
        else if (getModConfig().ListType_BeatSaver.GetValue() == "User") {
            searchViewController->SearchUser(currentSearchIndex);
        }
        else {
            searchViewController->loadingControl->ShowText("Invalid Selection for\nService BeatSaver!", false);
        }
    }
    else if (getModConfig().Service.GetValue() == "BeastSaber") {
        if (getModConfig().ListType_BeastSaber.GetValue() == "Curator Recommended") {
            searchViewController->SearchField->get_gameObject()->SetActive(false);
            searchViewController->GetCuratorRecommended(currentSearchIndex);
        }
        else if (getModConfig().ListType_BeastSaber.GetValue() == "Bookmarks") {
            searchViewController->GetBookmarks(currentSearchIndex);
        }
        else {
            searchViewController->loadingControl->ShowText("Invalid Selection for\nService BeastSaber!", false);
        }
    }
    else if (getModConfig().Service.GetValue() == "ScoreSaber") {
        if (getModConfig().ListType_ScoreSaber.GetValue() == "Trending") {
            searchViewController->GetTrending(currentSearchIndex);
        }
        else if (getModConfig().ListType_ScoreSaber.GetValue() == "Latest Ranked") {
            searchViewController->GetLatestRanked(currentSearchIndex);
        }
        else if (getModConfig().ListType_ScoreSaber.GetValue() == "Top Played") {
            searchViewController->SearchField->get_gameObject()->SetActive(false);
            searchViewController->GetTopPlayed(currentSearchIndex);
        }
        else if (getModConfig().ListType_ScoreSaber.GetValue() == "Top Ranked") {
            searchViewController->GetTopRanked(currentSearchIndex);
        }
        else {
            searchViewController->loadingControl->ShowText("Invalid Selection for\nService ScoreSaber!", false);
        }
    }
    else {
        searchViewController->loadingControl->ShowText("Invalid Selection\nselected Service Unknown!", false);
    }
}

void DownloadSongsSearchViewController::SetPage(int page) {
    DownloadSongsSearchViewController::searchPage = page;

    searchViewController->pageIncrement->set_Value(page + 1);
}

void DownloadSongsSearchViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (firstActivation) {
        searchViewController = this;
        get_gameObject()->AddComponent<Touchable*>();

        SearchField = CreateStringSetting(get_transform(), "Search", "", UnityEngine::Vector2(0.0f, 0.0f), UnityEngine::Vector3(0.0f, -38.0f, 0.0f),
            [this](StringW value) {
                DownloadSongsSearchViewController::SearchQuery = static_cast<std::string>(value);
                if (getModConfig().Service.GetValue() == "BeastSaber" && getModConfig().ListType_BeastSaber.GetValue() == "Bookmarks") getModConfig().BookmarkUsername.SetValue(std::string(value));
                Search();
            }
        );
        if (getModConfig().Service.GetValue() == "BeastSaber" && getModConfig().ListType_BeastSaber.GetValue() == "Bookmarks") {
            DownloadSongsSearchViewController::SearchQuery = getModConfig().BookmarkUsername.GetValue();
            searchViewController->SearchField->SetText(getModConfig().BookmarkUsername.GetValue());
        }
        auto container = CreateScrollView(get_transform());

        auto externalComponents = container->GetComponent<BSML::ExternalComponents*>();
        RectTransform* scrollTransform = externalComponents->Get<RectTransform*>();
        scrollTransform->set_anchoredPosition(UnityEngine::Vector2(0.0f, -1.0f));
        scrollTransform->set_sizeDelta(UnityEngine::Vector2(-54.0f, -12.0f));
        CreateEntries(container->get_transform());

        pageIncrement = CreateIncrementSetting(get_transform(), "", 0, 1, DownloadSongsSearchViewController::searchPage + 1, true, false, 1, 0, UnityEngine::Vector2(-60.0f, -72.0f),
            [this](float newValue){
                if(newValue - 1 != DownloadSongsSearchViewController::searchPage) {
                    DownloadSongsSearchViewController::searchPage = newValue - 1;
                    Search();
                }
            }
        );
        Object::Destroy(pageIncrement->GetComponentInChildren<TextMeshProUGUI*>());
        Object::Destroy(pageIncrement->GetComponentInChildren<LayoutElement*>());

        // LoadingControl has to be added after the ScrollView, as otherwise it will be behind it and the RefreshButton unselectable
        GameObject* existingLoadingControl = Resources::FindObjectsOfTypeAll<LoadingControl*>()->First()->get_gameObject();
        GameObject* loadingControlGameObject = UnityEngine::GameObject::Instantiate(existingLoadingControl, get_transform());
        auto loadingControlTransform = loadingControlGameObject->get_transform();
        loadingControlTransform->set_localPosition(Vector3(0.f, 0.0f, 0.0f));
        loadingControl = loadingControlGameObject->GetComponent<LoadingControl*>();
        loadingControl->add_didPressRefreshButtonEvent(custom_types::MakeDelegate<System::Action*>(
            (std::function<void()>) [this]() {
                Search();
            }
        ));
        loadingControl->_loadingText->set_text("Loading...");
        loadingControl->set_enabled(true);

        Search();
    }
}
