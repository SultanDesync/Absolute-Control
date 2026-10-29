#include "ui/PauseMenuIntegration.h"
#include "ui/PauseEntryPolicy.h"
#include "ui/MenuOpenOrigin.h"
#include "interop/OSFUICompatibility.h"

#include "EvidenceLog.h"
#include "ui/MenuMessaging.h"
#include "ui/PauseMenuPresentation.h"
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
#include "research/PauseMenuDiagnostics.h"
#endif

#include <RE/Starfield.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <string_view>

namespace AbsoluteControlPanelResearch::Ui::PauseMenuIntegration
{
    namespace
    {
        constexpr std::uint32_t kControlPanelPauseAction = PauseEntryPolicy::kAction;
        constexpr std::string_view kPauseEntryLabel = PauseEntryPolicy::kLabel;

        [[nodiscard]] std::optional<std::uint32_t> ReadAction(
            const RE::Scaleform::GFx::Value& value) noexcept
        {
            if (value.IsUInt()) return value.GetUInt();
            if (value.IsInt()) return PauseEntryPolicy::ParseAction(value.GetInt());
            if (value.IsNumber()) return PauseEntryPolicy::ParseAction(value.GetNumber());
            return std::nullopt;
        }

        [[nodiscard]] PauseEntryPolicy::Identity ReadIdentity(
            const RE::Scaleform::GFx::Value& entry)
        {
            RE::Scaleform::GFx::Value action;
            RE::Scaleform::GFx::Value owner;
            return {
                entry.GetMember("uActionType", &action) ? ReadAction(action) : std::nullopt,
                entry.GetMember(PauseEntryPolicy::kOwnerMarker, &owner) &&
                    owner.IsBoolean() && owner.GetBoolean()
            };
        }

        class EntryMemberCopyVisitor final :
            public RE::Scaleform::GFx::Value::ObjectVisitor
        {
        public:
            explicit EntryMemberCopyVisitor(RE::Scaleform::GFx::Value& a_target) noexcept :
                target_(a_target)
            {}

            void Visit(
                const char* a_name, const RE::Scaleform::GFx::Value& a_value) override
            {
                if (a_name && *a_name && target_.SetMember(a_name, a_value)) {
                    ++copiedMemberCount_;
                } else {
                    ++rejectedMemberCount_;
                }
            }

            [[nodiscard]] std::uint32_t CopiedMemberCount() const noexcept
            {
                return copiedMemberCount_;
            }

            [[nodiscard]] std::uint32_t RejectedMemberCount() const noexcept
            {
                return rejectedMemberCount_;
            }

        private:
            RE::Scaleform::GFx::Value& target_;
            std::uint32_t copiedMemberCount_{};
            std::uint32_t rejectedMemberCount_{};
        };

        enum class InjectionResult : std::uint8_t
        {
            NotReady,
            Injected,
            AlreadyPresent,
            Failed
        };

        std::atomic_bool g_lifecycleHookAvailable{};
        std::atomic_bool g_hostReady{};
        std::atomic<MenuOpenOrigin> g_queuedOrigin{ MenuOpenOrigin::Unclaimed };

        // Independent movie identity, listener and retry state for each native surface.
        // Neither title activity nor PauseMenu opens can invalidate the other callback.
        template <bool MainMenu>
        struct EntryIntegration
        {
        static inline bool g_wasInteractive{};
        static inline bool g_hadOwnedEntry{};
        static inline unsigned g_rebuildBudget{ 4 };
        static inline unsigned g_textLogBudget{ 8 };
        static void Log(std::string_view event, std::string_view detail = {})
        {
            if constexpr (MainMenu) {
                const auto suffix = event.starts_with("pause_") ? event.substr(6) : event;
                EvidenceLog::Event(std::string("main_menu_") + std::string(suffix), detail);
            } else {
                EvidenceLog::Event(event, detail);
            }
        }

        static bool MainPanelInteractive(RE::Scaleform::GFx::Movie* movie)
        {
            using Value = RE::Scaleform::GFx::Value;
            const auto ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::BSFixedString("MainMenu")) ||
                !movie || !movie->asMovieRoot) return false;
            Value panel, list, value;
            if (!movie->asMovieRoot->GetVariable(&panel, "_root.Menu_mc.MainPanel_mc") ||
                !panel.IsObject() || !panel.GetMember("visible", &value) ||
                !value.IsBoolean() || !value.GetBoolean() ||
                !panel.GetMember("MainList_mc", &list) || !list.IsObject()) return false;
            for (const char* key : { "disableInput", "disableSelection" }) {
                if (!list.GetMember(key, &value) || !value.IsBoolean() || value.GetBoolean())
                    return false;
            }
            return true;
        }

        // Title text fields auto-grow beyond their row. Constrain only our text
        // field to symmetric native padding before enabling glyph shrink. Do not
        // take the row, fill, border or Text_mc transform away from its timeline.
        static bool ConstrainTitleText(RE::Scaleform::GFx::Value& clip)
        {
            using Value = RE::Scaleform::GFx::Value;
            Value text, field, border, bounds;
            if (!clip.GetMember("Text_mc", &text) || !text.IsObject() ||
                !text.GetMember("textField", &field) || !field.IsObject() ||
                !clip.GetMember("Border_mc", &border) || !border.IsObject() ||
                !border.Invoke("getBounds", &bounds, &text, 1) || !bounds.IsObject()) return false;
            auto number = [](const Value& object, const char* key) {
                Value value;
                return object.GetMember(key, &value) && value.IsNumber() ? value.GetNumber() : NAN;
            };
            if (number(field, "scaleX") != 1.0 || number(field, "rotation") != 0.0) return false;
            const auto padding = number(field, "x") - number(bounds, "x");
            const auto width = number(bounds, "width") - 2.0 * padding;
            if (!std::isfinite(padding) || !std::isfinite(width) || padding < 0 ||
                width < 64 || width > 1000) return false;
            return field.SetMember("autoSize", Value("none")) && field.SetMember("width", Value(width));
        }

        static std::optional<bool> HasOwnedEntry(RE::Scaleform::GFx::Movie* movie)
        {
            using Value = RE::Scaleform::GFx::Value;
            Value list, count;
            if (!movie->asMovieRoot->GetVariable(&list, "_root.Menu_mc.MainPanel_mc.MainList_mc") ||
                !list.IsObject() || !list.GetMember("entryCount", &count) || !count.IsInt() ||
                count.GetInt() < 1 || count.GetInt() > 128) return {};
            for (int i = 0; i < count.GetInt(); ++i) {
                Value arg(i), row;
                if (!list.Invoke("GetDataForEntry", &row, &arg, 1) || !row.IsObject()) return {};
                const auto identity = ReadIdentity(row);
                if (identity.owned && identity.action == kControlPanelPauseAction) {
                    const bool disabled = !g_hostReady.load(std::memory_order_acquire);
                    Value wasDisabled;
                    const bool readinessChanged = row.GetMember("bDisabled", &wasDisabled) &&
                        wasDisabled.IsBoolean() && wasDisabled.GetBoolean() != disabled;
                    if (readinessChanged) (void)row.SetMember("bDisabled", Value(disabled));
                    // MainMenu's standard row auto-grows its field and does not enable
                    // TextFieldEx shrink by default. Bound only the owned text field;
                    // preserve the animated background, hit area and glyph container.
                    Value clip, options[]{ Value("shrink"), Value(false) }, configured, selected, fittedSelection;
                    if (list.Invoke("FindClipForEntry", &clip, &arg, 1) && clip.IsObject()) {
                        const bool alreadyConfigured = clip.GetMember("_absoluteControlTextConfigured", &configured) &&
                            configured.IsBoolean() && configured.GetBoolean();
                        const bool selectionChanged = clip.GetMember("selected", &selected) && selected.IsBoolean() &&
                            (!clip.GetMember("_absoluteControlTextSelected", &fittedSelection) ||
                             !fittedSelection.IsBoolean() || selected.GetBoolean() != fittedSelection.GetBoolean());
                        if (!alreadyConfigured || readinessChanged || selectionChanged) {
                            const bool constrained = ConstrainTitleText(clip);
                            const bool fitted = constrained && clip.Invoke("Configure", nullptr, options, 2);
                            const bool refreshed = clip.Invoke("SetEntryText", nullptr, &row, 1);
                            if (fitted && refreshed) {
                                (void)clip.SetMember("_absoluteControlTextConfigured", Value(true));
                                if (selected.IsBoolean()) (void)clip.SetMember("_absoluteControlTextSelected", selected);
                            }
                            if (g_textLogBudget > 0) {
                                --g_textLogBudget;
                                Value text, field, border;
                                (void)clip.GetMember("Text_mc", &text);
                                if (text.IsObject()) (void)text.GetMember("textField", &field);
                                (void)clip.GetMember("Border_mc", &border);
                                auto number = [](const Value& object, const char* key) {
                                    Value value;
                                    return object.IsObject() && object.GetMember(key, &value) && value.IsNumber() ? value.GetNumber() : -1.0;
                                };
                                Log("pause_entry_text_configured", std::format(
                                    "configured={} refreshed={} disabled={} field_width={} text_width={} text_height={} border_width={} text_x={} field_x={}",
                                    fitted, refreshed, disabled, number(field, "width"), number(field, "textWidth"),
                                    number(field, "textHeight"), number(border, "width"), number(text, "x"), number(field, "x")));
                            }
                        }
                    }
                    return true;
                }
            }
            return false;
        }

        static inline std::atomic_bool g_enablePauseEntry{};
        static inline std::atomic<RE::Scaleform::GFx::Movie*> g_expectedMovie{};
        static inline std::atomic<std::uint32_t> g_cycle{};
        static inline std::atomic<std::uint32_t> g_completedCycle{};
        static inline std::atomic<std::uint32_t> g_captureInstalledCycle{};
        static inline std::atomic<std::uint32_t> g_advanceAttemptCount{};
        static inline std::atomic<std::int32_t> g_lastAdvanceResult{ -1 };
        static inline std::atomic<std::uint32_t> g_requestedCommand{};
        struct ActiveMenuArraySnapshot
        {
            std::uint32_t count{};
            std::int32_t index{ -1 };
        };

        [[nodiscard]] static ActiveMenuArraySnapshot ReadActiveMenuArray(
            RE::UI* a_ui, RE::IMenu* a_menu) noexcept
        {
            ActiveMenuArraySnapshot result;
            if (!a_ui) {
                return result;
            }

            const auto uiBytes = reinterpret_cast<const std::byte*>(a_ui);
            result.count = *reinterpret_cast<const std::uint32_t*>(uiBytes + 0x430);
            const auto entries = *reinterpret_cast<RE::IMenu* const* const*>(
                uiBytes + 0x438);
            if (!entries || !a_menu) {
                return result;
            }
            for (std::uint32_t index = 0; index < result.count; ++index) {
                if (entries[index] == a_menu) {
                    result.index = static_cast<std::int32_t>(index);
                    break;
                }
            }
            return result;
        }

        class PauseEntryCaptureHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                if (a_params.argCount != 1 || !a_params.args[0].IsObject()) {
                    Log(
                        "pause_entry_capture_rejected", "reason=invalid_event");
                    return;
                }

                RE::Scaleform::GFx::Value payload;
                RE::Scaleform::GFx::Value action;
                const bool resolved = a_params.args[0].GetMember("params", &payload) &&
                                      payload.IsObject() &&
                                      payload.GetMember("entryAction", &action);
                const auto actionValue = resolved ? ReadAction(action) : std::nullopt;
                if (actionValue != kControlPanelPauseAction || !a_params.movie ||
                    a_params.movie != g_expectedMovie.load(std::memory_order_acquire) ||
                    !a_params.movie->asMovieRoot) {
                    return;
                }

                RE::Scaleform::GFx::Value list;
                RE::Scaleform::GFx::Value selected;
                if (!a_params.movie->asMovieRoot->GetVariable(
                        &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") ||
                    !list.IsObject() || !list.GetMember("selectedEntry", &selected) ||
                    !selected.IsObject() || !PauseEntryPolicy::Handles(actionValue,
                        ReadIdentity(selected), g_enablePauseEntry.load(std::memory_order_acquire))) {
                    return;
                }

                if constexpr (MainMenu) {
                    if (!MainPanelInteractive(a_params.movie)) return;
                }
                const bool stopped =
                    a_params.args[0].Invoke("stopImmediatePropagation");
                Log(
                    "pause_entry_activated",
                    std::format(
                        "action=0x{:08X} propagation_stopped={}",
                        *actionValue, stopped));
                if (!stopped) return;
                if (!g_hostReady.load(std::memory_order_acquire)) {
                    Log("pause_entry_open_rejected", "reason=host-not-ready");
                    return;
                }
                if (Interop::OSFUICompatibility::IsPresentationActive(RE::UI::GetSingleton())) {
                    Log("pause_entry_open_rejected", "reason=foreign-presentation-active");
                    return;
                }
                // Keep the native PauseMenu resident as the gameplay-pause owner while
                // the higher-priority Control Panel covers it. Hiding it before the
                // panel's Show exposed gameplay for a frame. The custom panel takes
                // its own balanced native PauseMenu audio-mode lease on Show because
                // Starfield's MenuAudioHandler recognizes only hard-coded menu names.
                Log(
                    "pause_entry_underlay_retained",
                    MainMenu ? "menu=MainMenu target=AbsoluteControlPanelMenu" :
                        "menu=PauseMenu target=AbsoluteControlPanelMenu");
                Ui::QueueControlPanelMessage(
                    RE::UI_MESSAGE_TYPE::kShow, MainMenu ? "main-menu-entry" : "pause-entry");
            }
        };

        [[nodiscard]] static PauseEntryCaptureHandler* GetCaptureHandler()
        {
            static auto handler =
                RE::Scaleform::make_shared<PauseEntryCaptureHandler>();
            return handler.get();
        }

        [[nodiscard]] static InjectionResult InjectPauseMenuEntry(
            RE::Scaleform::GFx::Movie* a_movie, std::uint32_t a_commandId,
            std::uint32_t a_cycle) noexcept
        {
            constexpr std::int32_t kMaximumPauseEntries = 128;
            try {
                if (!a_movie || !a_movie->asMovieRoot) {
                    return InjectionResult::NotReady;
                }

                auto* movieRoot = a_movie->asMovieRoot.get();
                RE::Scaleform::GFx::Value list;
                if (!movieRoot->GetVariable(
                        &list, "_root.Menu_mc.MainPanel_mc.MainList_mc") ||
                    !list.IsObject()) {
                    return InjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entryCountValue;
                RE::Scaleform::GFx::Value selectedIndexValue;
                const bool gotEntryCount = list.GetMember("entryCount", &entryCountValue);
                const bool gotSelectedIndex =
                    list.GetMember("selectedIndex", &selectedIndexValue);
                if (!gotEntryCount || !entryCountValue.IsInt() || !gotSelectedIndex ||
                    !selectedIndexValue.IsInt()) {
                    return InjectionResult::NotReady;
                }

                const auto entryCount = entryCountValue.GetInt();
                const auto originalIndex = selectedIndexValue.GetInt();
                if (entryCount <= 0 || entryCount > kMaximumPauseEntries) {
                    return InjectionResult::NotReady;
                }

                RE::Scaleform::GFx::Value entries;
                RE::Scaleform::GFx::Value nativeEntryTemplate;
                movieRoot->CreateArray(&entries);
                std::array<PauseEntryPolicy::Identity, kMaximumPauseEntries> identities{};
                bool cloned = entries.IsArray();
                bool gotNativeEntryTemplate = false;
                for (std::int32_t index = 0; cloned && index < entryCount; ++index) {
                    RE::Scaleform::GFx::Value indexValue(index);
                    RE::Scaleform::GFx::Value entry;
                    cloned = list.Invoke("GetDataForEntry", &entry, &indexValue, 1) && entry.IsObject();
                    if (!cloned) {
                        break;
                    }

                    identities[index] = ReadIdentity(entry);
                    // Retain the existing row data shape for native styling.
                    // Existing rows themselves are passed through unchanged.
                    nativeEntryTemplate = entry;
                    gotNativeEntryTemplate = true;
                    cloned = entries.PushBack(entry);
                }
                const bool snapshotSelectionRestored = true; // Snapshot accessor does not move selection.
                if (!cloned || !snapshotSelectionRestored) {
                    Log(
                        "pause_entry_injection_rejected",
                        std::format(
                            "cycle={} id={} reason=snapshot_failed selection_restored={}",
                            a_cycle, a_commandId, snapshotSelectionRestored));
                    return InjectionResult::Failed;
                }
                const auto admission = PauseEntryPolicy::Inspect(
                    std::span{ identities }.first(static_cast<std::size_t>(entryCount)));
                if (admission == PauseEntryPolicy::Admission::Conflict) {
                    Log("pause_entry_injection_rejected",
                        std::format("cycle={} id={} reason=entry_identity_conflict",
                            a_cycle, a_commandId));
                    return InjectionResult::Failed;
                }
                if (admission == PauseEntryPolicy::Admission::AlreadyPresent) {
                    Log(
                        "pause_entry_injection_skipped",
                        std::format(
                            "cycle={} id={} reason=already_present", a_cycle, a_commandId));
                    return InjectionResult::AlreadyPresent;
                }

                if (g_captureInstalledCycle.load(std::memory_order_acquire) != a_cycle) {
                    RE::Scaleform::GFx::Value menuRoot;
                    const bool gotMenuRoot = movieRoot->GetVariable(
                        &menuRoot, "_root.Menu_mc") && menuRoot.IsObject();
                    RE::Scaleform::GFx::Value installedMarker;
                    const bool listenerAlreadyInstalled = gotMenuRoot &&
                        menuRoot.GetMember(
                            "_absoluteControlPanelCaptureInstalled", &installedMarker) &&
                        installedMarker.IsBoolean() && installedMarker.GetBoolean();
                    if (listenerAlreadyInstalled) {
                        g_captureInstalledCycle.store(a_cycle, std::memory_order_release);
                        Log(
                            "pause_entry_capture_reused",
                            std::format("cycle={} id={}", a_cycle, a_commandId));
                    }
                    auto callbackType =
                        RE::Scaleform::GFx::Value::ValueType::kUndefined;
                    bool callbackUsable = false;
                    bool listenerInvoked = false;
                    if (gotMenuRoot && !listenerAlreadyInstalled) {
                        RE::Scaleform::GFx::Value eventName;
                        RE::Scaleform::GFx::Value callback;
                        movieRoot->CreateString(&eventName, "MainPanel_EntryPress");
                        movieRoot->CreateFunction(&callback, GetCaptureHandler());
                        callbackType = callback.GetType();
                        std::array<RE::Scaleform::GFx::Value, 5> listenerArgs{
                            eventName, callback, RE::Scaleform::GFx::Value(true),
                            RE::Scaleform::GFx::Value(std::int32_t{ 1000 }),
                            RE::Scaleform::GFx::Value(false)
                        };
                        callbackUsable = callback.IsObject() ||
                            callback.GetType() ==
                                RE::Scaleform::GFx::Value::ValueType::kClosure;
                        listenerInvoked = callbackUsable && menuRoot.Invoke(
                            "addEventListener", nullptr, listenerArgs.data(),
                            listenerArgs.size());
                    }
                    if (!listenerAlreadyInstalled) {
                        const bool listenerInstalled = callbackUsable && listenerInvoked;
                        const bool markerSet = listenerInstalled && menuRoot.SetMember(
                            "_absoluteControlPanelCaptureInstalled",
                            RE::Scaleform::GFx::Value(true));
                        Log(
                            "pause_entry_capture_installed",
                            std::format(
                                "cycle={} id={} menu_root={} callback_type={} "
                                "callback_usable={} listener_invoked={} installed={} "
                                "marker_set={}",
                                a_cycle, a_commandId, gotMenuRoot,
                                static_cast<std::uint32_t>(callbackType), callbackUsable,
                                listenerInvoked, listenerInstalled, markerSet));
                        if (!listenerInstalled) {
                            return InjectionResult::Failed;
                        }
                        g_captureInstalledCycle.store(a_cycle, std::memory_order_release);
                    }
                }

                RE::Scaleform::GFx::Value entry;
                RE::Scaleform::GFx::Value label;
                RE::Scaleform::GFx::Value confirmText;
                movieRoot->CreateObject(&entry);
                EntryMemberCopyVisitor templateCopy(entry);
                if (gotNativeEntryTemplate) {
                    nativeEntryTemplate.VisitMembers(&templateCopy);
                }
                movieRoot->CreateString(&label, kPauseEntryLabel.data());
                movieRoot->CreateString(&confirmText, "");
                const bool populated = entry.IsObject() &&
                    entry.SetMember(PauseEntryPolicy::kOwnerMarker,
                        RE::Scaleform::GFx::Value(true)) &&
                    entry.SetMember("sActionText", label) &&
                    entry.SetMember("sConfirmText", confirmText) &&
                    entry.SetMember("uActionType", RE::Scaleform::GFx::Value(kControlPanelPauseAction)) &&
                    entry.SetMember("bDisabled", RE::Scaleform::GFx::Value(!g_hostReady.load(std::memory_order_acquire))) &&
                    entry.SetMember("bHasDisabledAction", RE::Scaleform::GFx::Value(false)) &&
                    entry.SetMember("bHasNotification", RE::Scaleform::GFx::Value(false)) &&
                    entry.SetMember("bShowSpinner", RE::Scaleform::GFx::Value(false)) &&
                    entries.PushBack(entry);
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
                if constexpr (!MainMenu) Research::PauseMenuDiagnostics::Observe(a_movie, a_cycle, "before_ac_publish");
#endif
                const bool invoked = populated &&
                    list.Invoke("InitializeEntries", nullptr, &entries, 1);
                RE::Scaleform::GFx::Value indexAfter;
                const bool restored = list.GetMember("selectedIndex", &indexAfter) &&
                    indexAfter.IsInt() && indexAfter.GetInt() == originalIndex;
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
                if constexpr (!MainMenu) Research::PauseMenuDiagnostics::Observe(a_movie, a_cycle, "after_ac_publish");
#endif
                Log(
                    invoked ? "pause_entry_injected" : "pause_entry_injection_rejected",
                    std::format(
                        "cycle={} id={} existing_count={} requested_count={} action=0x{:08X} "
                        "template={} members_copied={} members_rejected={} label={} "
                        "populated={} invoked={} selection_restored={}",
                        a_cycle, a_commandId, entryCount, entryCount + 1,
                        kControlPanelPauseAction, gotNativeEntryTemplate,
                        templateCopy.CopiedMemberCount(),
                        templateCopy.RejectedMemberCount(), kPauseEntryLabel,
                        populated, invoked, restored));
                return invoked ? InjectionResult::Injected : InjectionResult::Failed;
            } catch (...) {
                Log(
                    "pause_entry_injection_error",
                    std::format("cycle={} id={}", a_cycle, a_commandId));
                return InjectionResult::Failed;
            }
        }

        class PauseEntryAdvanceHandler final :
            public RE::Scaleform::GFx::FunctionHandler
        {
        public:
            void Call(const Params& a_params) override
            {
                constexpr std::uint32_t kMaximumAdvanceAttempts = 180;
                const auto cycle = g_cycle.load(std::memory_order_acquire);
                const auto expectedMovie = g_expectedMovie.load(std::memory_order_acquire);
                const auto requestedCommand =
                    g_requestedCommand.exchange(0, std::memory_order_acq_rel);
                if (!cycle || !a_params.movie || a_params.movie != expectedMovie) {
                    return;
                }
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
                if constexpr (!MainMenu) Research::PauseMenuDiagnostics::Advance(a_params.movie, cycle);
#endif
                if (!g_enablePauseEntry.load(std::memory_order_acquire)) return;
                if constexpr (MainMenu) {
                    const bool interactive = MainPanelInteractive(a_params.movie);
                    if (!interactive) { g_wasInteractive = false; return; }
                    if (!g_wasInteractive) {
                        // Re-arm once on a real panel-interactivity transition (settings/load
                        // return), never continuously compete with another injector.
                        g_completedCycle.store(0, std::memory_order_release);
                        g_advanceAttemptCount.store(0, std::memory_order_release);
                        g_lastAdvanceResult.store(-1, std::memory_order_release);
                        g_rebuildBudget = 4;
                        g_hadOwnedEntry = false;
                    }
                    g_wasInteractive = true;
                    if (g_hadOwnedEntry && g_completedCycle.load(std::memory_order_acquire) == cycle) {
                        const auto present = HasOwnedEntry(a_params.movie);
                        if (present && !*present) {
                            g_hadOwnedEntry = false;
                            if (g_rebuildBudget > 0) {
                                --g_rebuildBudget;
                                g_completedCycle.store(0, std::memory_order_release);
                                g_advanceAttemptCount.store(0, std::memory_order_release);
                                Log("pause_entry_rebuild_seen", std::format("remaining={}", g_rebuildBudget));
                            } else {
                                Log("pause_entry_rebuild_stopped", "reason=republication-budget-exhausted");
                            }
                        }
                    }
                }
                if constexpr (!MainMenu) PauseMenuPresentation::Advance(a_params.movie, cycle);
                if (!requestedCommand &&
                    g_completedCycle.load(std::memory_order_acquire) == cycle) {
                    return;
                }
                if (requestedCommand) {
                    g_completedCycle.store(0, std::memory_order_release);
                    g_advanceAttemptCount.store(0, std::memory_order_release);
                    g_lastAdvanceResult.store(-1, std::memory_order_release);
                }

                const auto attempt =
                    g_advanceAttemptCount.fetch_add(1, std::memory_order_acq_rel) + 1;
                const auto commandId = requestedCommand ? requestedCommand : 1000000 + cycle;
                const auto result = InjectPauseMenuEntry(a_params.movie, commandId, cycle);
                if constexpr (!MainMenu) PauseMenuPresentation::Advance(a_params.movie, cycle);
                const auto resultValue = static_cast<std::int32_t>(result);
                const auto priorResult =
                    g_lastAdvanceResult.exchange(resultValue, std::memory_order_acq_rel);
                if (attempt == 1 || priorResult != resultValue) {
                    Log(
                        "pause_entry_advance_tick",
                        std::format(
                            "cycle={} attempt={} result={} movie=0x{:X}", cycle,
                            attempt, resultValue,
                            reinterpret_cast<std::uintptr_t>(a_params.movie)));
                }

                if (result == InjectionResult::Injected ||
                    result == InjectionResult::AlreadyPresent) {
                    if constexpr (MainMenu) g_hadOwnedEntry = true;
                    g_completedCycle.store(cycle, std::memory_order_release);
                    Log(
                        "pause_entry_advance_completed",
                        std::format("cycle={} attempts={}", cycle, attempt));
                } else if (result == InjectionResult::Failed) {
                    // Do not repeatedly compete with a foreign row or list contract.
                    g_completedCycle.store(cycle, std::memory_order_release);
                    Log("pause_entry_advance_stopped",
                        std::format("cycle={} attempts={} reason=incompatible_list",
                            cycle, attempt));
                } else if (attempt >= kMaximumAdvanceAttempts) {
                    g_completedCycle.store(cycle, std::memory_order_release);
                    Log(
                        "pause_entry_advance_timeout",
                        std::format(
                            "cycle={} attempts={} last_result={}", cycle, attempt,
                            resultValue));
                }
            }
        };

        [[nodiscard]] static PauseEntryAdvanceHandler* GetAdvanceHandler()
        {
            static auto handler =
                RE::Scaleform::make_shared<PauseEntryAdvanceHandler>();
            return handler.get();
        }

        static void OnPauseMenuInserted(RE::IMenu* a_menu) noexcept
        {
            auto* movie = a_menu && a_menu->uiMovie ? a_menu->uiMovie.get() : nullptr;
            if (!movie || !movie->asMovieRoot) {
                Log(
                    "pause_entry_boundary_rejected", "reason=movie_unavailable");
                return;
            }

            const auto cycle = g_cycle.fetch_add(1, std::memory_order_acq_rel) + 1;
            g_expectedMovie.store(movie, std::memory_order_release);
            g_wasInteractive = false;
            g_hadOwnedEntry = false;
            g_rebuildBudget = 4;
            g_textLogBudget = 8;
            g_completedCycle.store(0, std::memory_order_release);
            g_captureInstalledCycle.store(0, std::memory_order_release);
            g_advanceAttemptCount.store(0, std::memory_order_release);
            g_lastAdvanceResult.store(-1, std::memory_order_release);
            Log(
                "pause_entry_boundary_reached",
                std::format(
                    "cycle={} menu=0x{:X} movie=0x{:X}", cycle,
                    reinterpret_cast<std::uintptr_t>(a_menu),
                    reinterpret_cast<std::uintptr_t>(movie)));

            auto* movieRoot = movie->asMovieRoot.get();
            RE::Scaleform::GFx::Value eventTarget;
            const bool menuTarget = movieRoot->GetVariable(
                &eventTarget, "_root.Menu_mc") && eventTarget.IsObject();
            const bool rootTarget = menuTarget ||
                (movieRoot->GetVariable(&eventTarget, "_root") && eventTarget.IsObject());
            RE::Scaleform::GFx::Value installedMarker;
            const bool listenerAlreadyInstalled = rootTarget && eventTarget.GetMember(
                "_absoluteControlPanelAdvanceInstalled", &installedMarker) &&
                installedMarker.IsBoolean() && installedMarker.GetBoolean();
            if (listenerAlreadyInstalled) {
                Log(
                    "pause_entry_advance_listener_reused",
                    std::format(
                        "cycle={} target={} movie=0x{:X}", cycle,
                        menuTarget ? "menu" : "root",
                        reinterpret_cast<std::uintptr_t>(movie)));
                return;
            }
            RE::Scaleform::GFx::Value eventName;
            RE::Scaleform::GFx::Value callback;
            movieRoot->CreateString(&eventName, "enterFrame");
            movieRoot->CreateFunction(&callback, GetAdvanceHandler());
            const bool callbackUsable = callback.IsObject() ||
                callback.GetType() == RE::Scaleform::GFx::Value::ValueType::kClosure;
            std::array<RE::Scaleform::GFx::Value, 5> listenerArgs{
                eventName, callback, RE::Scaleform::GFx::Value(false),
                RE::Scaleform::GFx::Value(std::int32_t{ 1000 }),
                RE::Scaleform::GFx::Value(false)
            };
            const bool invoked = rootTarget && callbackUsable && eventTarget.Invoke(
                "addEventListener", nullptr, listenerArgs.data(), listenerArgs.size());
            const bool markerSet = invoked && eventTarget.SetMember(
                "_absoluteControlPanelAdvanceInstalled", RE::Scaleform::GFx::Value(true));
            Log(
                invoked ? "pause_entry_advance_listener_installed" :
                          "pause_entry_advance_listener_rejected",
                std::format(
                    "cycle={} target={} callback_type={} callback_usable={} invoked={} "
                    "marker_set={}",
                    cycle, menuTarget ? "menu" : (rootTarget ? "root" : "none"),
                    static_cast<std::uint32_t>(callback.GetType()), callbackUsable,
                    invoked, markerSet));
            if (!invoked) {
                g_completedCycle.store(cycle, std::memory_order_release);
            }
        }

        };
        using PauseIntegration = EntryIntegration<false>;
        using MainIntegration = EntryIntegration<true>;

        struct ActiveMenuInsertHook
        {
            using func_t = void (*)(RE::UI*, RE::IMenu**);
            static inline func_t original{};

            static void Thunk(RE::UI* a_ui, RE::IMenu** a_menuSlot) noexcept
            {
                auto* menu = a_menuSlot ? *a_menuSlot : nullptr;
                const auto before = PauseIntegration::ReadActiveMenuArray(a_ui, menu);
                const auto name = menu ? menu->GetName() : "<null>";
                const bool isPauseMenu =
                    std::string_view{ name ? name : "" } == "PauseMenu";
                const bool isMainMenu = std::string_view{ name ? name : "" } == "MainMenu";
                const auto flags = menu ? menu->flags : 0;
                const auto priority = menu ? *(
                    reinterpret_cast<const std::uint8_t*>(menu) + 0x110) : 0;
                EvidenceLog::Event(
                    "active_menu_insert_entered",
                    std::format(
                        "name={} menu=0x{:X} count={} index={} priority={} flags=0x{:08X}",
                        name ? name : "<null>", reinterpret_cast<std::uintptr_t>(menu),
                        before.count, before.index, priority, flags));

                original(a_ui, a_menuSlot);

                menu = a_menuSlot ? *a_menuSlot : menu;
                const auto after = PauseIntegration::ReadActiveMenuArray(a_ui, menu);
                EvidenceLog::Event(
                    "active_menu_insert_completed",
                    std::format(
                        "name={} menu=0x{:X} count={} index={}", name ? name : "<null>",
                        reinterpret_cast<std::uintptr_t>(menu), after.count, after.index));

                if (isPauseMenu &&
#if defined(ACP_ENABLE_RESEARCH_TOOLS)
                    true) {
#else
                    PauseIntegration::g_enablePauseEntry.load(std::memory_order_acquire)) {
#endif
                    PauseIntegration::OnPauseMenuInserted(menu);
                }
                if (isMainMenu && MainIntegration::g_enablePauseEntry.load(std::memory_order_acquire)) {
                    MainIntegration::OnPauseMenuInserted(menu);
                }
            }
        };
    }

    bool InstallLifecycleHook(bool a_enablePauseEntry, bool a_enableMainMenuEntry) noexcept
    {
        PauseIntegration::g_enablePauseEntry.store(a_enablePauseEntry, std::memory_order_release);
        MainIntegration::g_enablePauseEntry.store(a_enableMainMenuEntry, std::memory_order_release);
        if (g_lifecycleHookAvailable.load(std::memory_order_acquire)) return true;
        constexpr std::uintptr_t kCallsiteRva = 0x0254181C;
        constexpr std::uintptr_t kExpectedTargetRva = 0x0253EF10;
        const auto imageBase = REX::FModule::GetExecutingModule().GetBaseAddress();
        const auto callsite = imageBase + kCallsiteRva;
        if (*reinterpret_cast<const std::uint8_t*>(callsite) != 0xE8) {
            EvidenceLog::Event(
                "lifecycle_trace_rejected", "successful-show callsite is not rel32 call");
            return false;
        }
        const auto displacement = *reinterpret_cast<const std::int32_t*>(callsite + 1);
        const auto target = callsite + 5 + displacement;
        if (target != imageBase + kExpectedTargetRva) {
            EvidenceLog::Event(
                "lifecycle_trace_rejected",
                std::format(
                    "actual_target_rva=0x{:08X} expected_target_rva=0x{:08X}",
                    target - imageBase, kExpectedTargetRva));
            return false;
        }

        ActiveMenuInsertHook::original = reinterpret_cast<ActiveMenuInsertHook::func_t>(
            REL::GetTrampoline().write_call<5>(callsite, &ActiveMenuInsertHook::Thunk));
        g_lifecycleHookAvailable.store(true, std::memory_order_release);
        EvidenceLog::Event(
            "lifecycle_trace_installed",
            std::format(
                "callsite_rva=0x{:08X} target_rva=0x{:08X}", kCallsiteRva,
                kExpectedTargetRva));
        return true;
    }

    void SetEntryEnabled(bool a_enabled) noexcept
    {
        PauseIntegration::g_enablePauseEntry.store(a_enabled, std::memory_order_release);
        EvidenceLog::Event("pause_entry_setting_changed",
            std::format("enabled={}", a_enabled));
    }

    void MarkHostReady() noexcept
    {
        g_hostReady.store(true, std::memory_order_release);
        EvidenceLog::Event("menu_entry_host_ready");
    }

    bool IsEntryEnabled() noexcept
    {
        return PauseIntegration::g_enablePauseEntry.load(std::memory_order_acquire);
    }

    void LogRegistration(bool a_enablePauseEntry) noexcept
    {
        if (!a_enablePauseEntry) {
            EvidenceLog::Event("pause_entry_integration_disabled");
            return;
        }
        if (!g_lifecycleHookAvailable.load(std::memory_order_acquire)) {
            EvidenceLog::Event("pause_entry_integration_unavailable",
                "host_available=true recovery=provider-open-or-configured-hotkey");
            return;
        }
        EvidenceLog::Event(
            "pause_entry_integration_registered",
            "mode=active-menu-boundary+scaleform-advance fail_closed=true recovery_hotkey=opt-in");
    }

    void RequestInjection(std::uint32_t a_commandId) noexcept
    {
        PauseIntegration::g_requestedCommand.store(a_commandId, std::memory_order_release);
    }

    void AttachExistingMainMenu() noexcept
    {
        if (!g_lifecycleHookAvailable.load(std::memory_order_acquire) ||
            !MainIntegration::g_enablePauseEntry.load(std::memory_order_acquire)) return;
        const auto tasks = SFSE::GetTaskInterface();
        if (!tasks) {
            EvidenceLog::Event("main_menu_attach_rejected", "reason=task-interface-unavailable");
            return;
        }
        try {
        tasks->AddTask([] {
            const auto ui = RE::UI::GetSingleton();
            const RE::BSFixedString name("MainMenu");
            auto menu = ui ? ui->GetMenu(name) : nullptr;
            if (ui && ui->IsMenuOpen(name) && menu && menu->uiMovie &&
                MainIntegration::g_expectedMovie.load(std::memory_order_acquire) != menu->uiMovie.get())
                MainIntegration::OnPauseMenuInserted(menu.get());
        });
        } catch (...) {
            EvidenceLog::Event("main_menu_attach_rejected", "reason=task-enqueue-failed");
        }
    }

    bool SetOpenOrigin(MenuOpenOrigin origin) noexcept
    {
        auto expected = MenuOpenOrigin::Unclaimed;
        return g_queuedOrigin.compare_exchange_strong(expected, origin, std::memory_order_acq_rel);
    }

    MenuOpenOrigin ConsumeOpenOrigin() noexcept
    {
        const auto origin = g_queuedOrigin.exchange(MenuOpenOrigin::Unclaimed, std::memory_order_acq_rel);
        return origin == MenuOpenOrigin::Unclaimed ? MenuOpenOrigin::None : origin;
    }
}
