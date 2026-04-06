#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GManager.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

using namespace geode::prelude;

/*static auto patch = Mod::get()->patch(reinterpret_cast<void*>(base::getCocos() + 0xA35CE), {0x31, 0xD2, 0x90});

$on_mod(Loaded) {
    if(patch.isOk()) {
        (void) patch.unwrap()->disable();
    }
}

void enablePatch() {
    if(patch.isOk()) {
        if(patch.unwrap()->enable()) {
            log::info("Patch applied successfully");
        } else {
            log::error("Failed to apply patch, quick saving on reset may not work");
        }
    } else {
        log::error("Failed to apply patch, quick saving on reset may not work");
    }
}

void disablePatch() {
    if(patch.isOk()) {
        if(patch.unwrap()->disable()) {
            log::info("Patch disabled successfully");
        } else {
            log::error("Failed to disable patch");
        }
    } else {
        log::error("Failed to disable patch");
    }
}*/

class $modify(FAPlayLayer, PlayLayer) {
    struct Fields {
        std::shared_ptr<DS_Dictionary> m_dict = std::make_shared<DS_Dictionary>();
        std::atomic_bool m_isSaving = false;
        bool m_isLoaded = false;
        bool m_didFinalSave = false;
    };

    /**
     * Utils
     */
    void saveDictToFile() {
        if(m_fields->m_isSaving) return;

        auto now = asp::Instant::now();
        auto filename = GameManager::get()->m_fileName;

        m_fields->m_isSaving = true;

        async::spawn(arc::spawnBlocking<void>([dict = m_fields->m_dict, filename] {
            #if defined(GEODE_IS_MACOS) || defined(GEODE_IS_IOS)
                auto str = dict->saveRootSubDictToString();
                PlatformToolbox::saveAndEncryptStringToFile(str, filename.c_str(), "/data/data/com.robtopx.geometryjump/");
            #else
                dict->saveRootSubDictToCompressedFile(filename.c_str());
            #endif
        }), [selfPtr = WeakRef(this), now] {
            if(auto self = selfPtr.lock()) {
                modify_cast<FAPlayLayer*>(self.data())->m_fields->m_isSaving = false;
            }
            log::debug("Saving took {}", now.elapsed());
        });
    }

    void fullDictSave(float dt) {
        if(m_fields->m_didFinalSave) return;

        m_fields->m_didFinalSave = true;
        GameManager::get()->encodeDataTo(m_fields->m_dict.get());
        saveDictToFile();
    }

    /**
     * Hooks
     */

    void levelComplete() {   
        PlayLayer::levelComplete();

        this->scheduleOnce(schedule_selector(FAPlayLayer::fullDictSave), 10.f);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        m_fields->m_isLoaded = true;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        
        GameManager::get()->encodeDataTo(m_fields->m_dict.get());

        return true;
    }

    void resetLevel() {
        static int lastPercentage = 0;

        if(!m_fields->m_isLoaded) return PlayLayer::resetLevel();

        if(m_level->m_newNormalPercent2 != lastPercentage) {
            auto start = asp::Instant::now();
            auto dict = m_fields->m_dict.get();

            auto GLM = GameLevelManager::get();
            auto GSM = GameStatsManager::get();

            dict->setDictForKey("GS_value", GSM->m_playerStats);
            dict->setIntegerForKey("GS_20", GSM->m_bonusKey);

            switch(m_level->m_levelType) {
                case GJLevelType::Editor: {
                    //TODO
                    break;
                }
                case GJLevelType::Main: {
                    dict->setDictForKey("GLM_01", GLM->m_mainLevels);
                    break;
                }
                case GJLevelType::Saved: {
                    bool isGLM03 = GLM->m_onlineLevels->objectForKey(fmt::to_string(m_level->m_levelID.value()).c_str()) == m_level;
                    bool isGLM10 = GLM->m_dailyLevels->objectForKey(fmt::to_string(m_level->m_levelID.value()).c_str()) == m_level;
                    bool isGLM16 = GLM->m_gauntletLevels->objectForKey(fmt::to_string(m_level->m_levelID.value()).c_str()) == m_level;
                    const char* dictKey = isGLM03 ? "GLM_03" : isGLM10 ? "GLM_10" : isGLM16 ? "GLM_16" : nullptr;
                    
                    if(!dictKey) break;

                    if(dict->stepIntoSubDictWithKey(dictKey)) {
                        dict->setObjectForKey(fmt::to_string(m_level->m_levelID.value()).c_str(), m_level);
                        dict->stepOutOfSubDict();
                    }

                    if(isGLM03) {
                        dict->setDictForKey("GS_7", GSM->m_onlineCurrencyScores);
                    } else if(isGLM10) {
                        dict->setDictForKey("GS_14", GSM->m_challengeDiamonds);
                        dict->setDictForKey("GS_16", GSM->m_timelyCurrencyScores);
                        dict->setDictForKey("GS_24", GSM->m_timelyDiamondScores);
                    } else if(isGLM16) {
                        dict->setDictForKey("GS_18", GSM->m_gauntletDiamondScores);
                        dict->setDictForKey("GS_23", GSM->m_gauntletCurrencyScores);
                    }
                    break;
                }
                default:
                    log::warn("Unknown level type, not saving progress");
                    break;
            }

            log::debug("Initializing save took {}", start.elapsed());
            saveDictToFile();

        }

        PlayLayer::resetLevel();
    }
};

// to avoid an annoying lag spike from double saving, we skip the quick save
// that happens in EndLevelLayer::onMenu, we save in levelComplete anyway,
// so no progress is lost
static bool g_shouldSkipQuickSave = false;
class $modify(GManager) {
    void save() {
        if(g_shouldSkipQuickSave) {
            g_shouldSkipQuickSave = false;
            return;
        }
        
        GManager::save();
    }
};

class $modify(EndLevelLayer) {
    void onMenu(CCObject* sender) {
        g_shouldSkipQuickSave = true;
        EndLevelLayer::onMenu(sender);
        auto pl = modify_cast<FAPlayLayer*>(PlayLayer::get());
        if(pl) {
            pl->fullDictSave(0);
        }
        g_shouldSkipQuickSave = false;
    }
};