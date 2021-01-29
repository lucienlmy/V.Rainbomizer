#include "missions_Flow.hh"

#include "CMath.hh"
#include "Utils.hh"
#include "common/logger.hh"
#include "mission/missions_Funcs.hh"
#include "mission/missions_Globals.hh"
#include "mission/missions_YscUtils.hh"
#include "missions.hh"
#include "scrThread.hh"
#include <cstdint>
#include <stdio.h>
#include <utility>

using MR = MissionRandomizer_Components;
template <auto &scrProgram_InitNativeTablese188>
bool
MissionRandomizer_Flow::ApplyCodeFixes (scrProgram *program)
{
    bool ret = scrProgram_InitNativeTablese188 (program);
    if (program->m_nScriptHash == "flow_controller"_joaat)
        {
            YscUtils utils (program);
            utils.FindCodePattern (
                "6f 54 ? ? 6f 54 ? ? 5d ? ? ? 5d ? ? ? 2e 01 00",
                [] (hook::pattern_match p) {
                    *p.get<uint32_t> (12) = 0;
                    Rainbomizer::Logger::LogMessage (
                        "Initialised mission_stat_watcher terminate fix");
                });
        }

    return ret;
}

void
MissionRandomizer_Flow::Reset ()
{
    static bool bHooksInitialised = false;
    if (!bHooksInitialised)
        {
            REGISTER_HOOK ("8b cb e8 ? ? ? ? 8b 43 70 ? 03 c4 a9 00 c0 ff ff",
                           2, ApplyCodeFixes, bool, scrProgram *);
            bHooksInitialised = true;
        }
    
    MR::sm_Order.Reset ();
    MR::sm_PlayerSwitcher.Reset ();

    OriginalMission   = nullptr;
    RandomizedMission = nullptr;
}

void
MissionRandomizer_Flow::InitStatWatcherForRandomizedMission ()
{
    uint32_t rId       = RandomizedMission->nId;
    auto     rDef_curr = RandomizedMission->pDef;
    auto     rDef_orig = &RandomizedMission->DefCopy;

    uint32_t prevFlags
        = std::exchange (rDef_curr->nMissionFlags, rDef_orig->nMissionFlags);

    Rainbomizer::Logger::LogMessage (
        "Initialising Stat Watcher for mission: %d", rId);

    YscFunctions::InitStatWatcherForMission (rId);
    rDef_curr->nMissionFlags = prevFlags;
}

bool
MissionRandomizer_Flow::PreMissionStart ()
{
    Rainbomizer::Logger::LogMessage ("Mission %s (%u) to %s (%u) ",
                                     OriginalMission->Data.sName,
                                     OriginalMission->nHash,
                                     RandomizedMission->Data.sName,
                                     RandomizedMission->nHash);

    nPlayerIndexBeforeMission = MR::sm_Globals.GetCurrentPlayer ();

    LogPlayerPos(true);
    MR::sm_PlayerSwitcher.BeginSwitch (
        RandomizedMission->Data.vecStartCoords,
        RandomizedMission->Data.vecStartCoords.GetRandomPlayer (),
        OriginalMission->Data.OrigFlags.NoTransition
            || RandomizedMission->Data.RandFlags.NoTransition);

    nLastPassedMissionTime = MR::sm_Globals.g_LastPassedMissionTime;
    bMissionRepeating      = false;
    bScriptEnded           = false;

    InitStatWatcherForRandomizedMission ();

    return true;
}

void
MissionRandomizer_Flow::HandleCurrentMissionChanges ()
{
    if (MR::sm_Globals.g_CurrentMission != nPreviousCurrentMission)
        {
            Rainbomizer::Logger::LogMessage (
                "g_CurrentMission changed: %u to %u", nPreviousCurrentMission,
                uint32_t (MR::sm_Globals.g_CurrentMission));

            nPreviousCurrentMission = MR::sm_Globals.g_CurrentMission;

            if (MR::sm_Globals.g_CurrentMission != -1u)
                {
                    OriginalMission = MR::sm_Order.GetMissionInfoFromId (
                        nPreviousCurrentMission);

                    if (OriginalMission)
                        RandomizedMission = MR::sm_Order.GetMissionInfo (
                            MR::sm_Order.GetRandomMissionHash (
                                OriginalMission->nHash));

                    if (RandomizedMission && PreMissionStart ())
                        return; // Success
                }

            // Failed
            if (!bScriptEnded || MR::sm_Globals.g_CurrentMission != -1u)
                {
                    OriginalMission   = nullptr;
                    RandomizedMission = nullptr;

                    bScriptEnded = false;
                }

            bMissionRepeating = false;
        }
}

bool
MissionRandomizer_Flow::OnMissionStart ()
{
    *MR::sm_Globals.FLAG_PLAYER_PED_INTRODUCED_M = true;
    *MR::sm_Globals.FLAG_PLAYER_PED_INTRODUCED_T = true;
    *MR::sm_Globals.FLAG_PLAYER_PED_INTRODUCED_F = true;
    *MR::sm_Globals.SP0_AVAILABLE                = true;
    *MR::sm_Globals.SP1_AVAILABLE                = true;
    *MR::sm_Globals.SP2_AVAILABLE                = true;

    YscFunctions::SetBuildingState (70, 1, 1, 1, 0);
    YscFunctions::SetBuildingState (71, 1, 1, 1, 0);
    YscFunctions::SetBuildingState (73, 1, 1, 1, 0);
    YscFunctions::SetBuildingState (72, 0, 1, 1, 0);
    YscFunctions::SetBuildingState (74, 0, 1, 1, 0);
    YscFunctions::SetDoorState (62, 1);
    YscFunctions::SetDoorState (63, 1);

    // Most missions need this to not get softlocked.
    if ("IS_CUTSCENE_ACTIVE"_n())
        {
            "REMOVE_CUTSCENE"_n();
            return false;
        }

    // Set player if mission was replayed
    if (bMissionRepeating
        && MR::sm_Globals.GetCurrentPlayer () == nPlayerIndexBeforeMission)
        {
            MR::sm_PlayerSwitcher.SetCurrentPlayer (
                RandomizedMission->Data.vecStartCoords.GetRandomPlayer ());
        }

    if (OriginalMission->Data.OrigFlags.FadeIn
        || OriginalMission->Data.RandFlags.FadeIn)
        {
            "SHUTDOWN_LOADING_SCREEN"_n();
            "DO_SCREEN_FADE_IN"_n(0);
        }

    // Need to do this again for mission fails.
    InitStatWatcherForRandomizedMission ();

    return true;
}

void
MissionRandomizer_Flow::LogPlayerPos (bool start)
{
    FILE *f    = (start) ? mStartCoordsFile : mEndCoordsFile;
    auto *info = (start) ? OriginalMission : RandomizedMission;

    Vector3_native pos = NativeManager::InvokeNative<Vector3_native> (
        "GET_ENTITY_COORDS"_joaat, "PLAYER_PED_ID"_n(), 0);
    float heading = "GET_ENTITY_HEADING"_n("PLAYER_PED_ID"_n());

    fprintf (f, "%s: %f %f %f %f\n", info->Data.sName, pos.x, pos.y, pos.z,
             heading);
    fflush (f);
}

bool
MissionRandomizer_Flow::OnMissionEnd (bool pass)
{
    bCallMissionEndNextFrame = false;

    Rainbomizer::Logger::LogMessage (
        "Mission %s, g_LastPassedMission = %u, g_CurrentMission = %u,"
        "g_LastPassedMissionTime = %u (prev = %u)",
        pass ? "Passed" : "Failed",
        uint32_t (MR::sm_Globals.g_LastPassedMission), nPreviousCurrentMission,
        uint32_t (MR::sm_Globals.g_LastPassedMissionTime),
        nLastPassedMissionTime);

    if (pass)
        {
            LogPlayerPos (false);

            MR::sm_PlayerSwitcher.BeginSwitch (
                OriginalMission->Data.vecEndCoords,
                OriginalMission->Data.vecEndCoords.GetRandomPlayer (),
                OriginalMission->Data.OrigFlags.NoTransition
                    || RandomizedMission->Data.RandFlags.NoTransition);
        }
    else
        {
            if (nPlayerIndexBeforeMission != ePlayerIndex::PLAYER_UNKNOWN)
                MR::sm_PlayerSwitcher.SetCurrentPlayer (
                    nPlayerIndexBeforeMission);
        }

    bMissionRepeating = true;
    bScriptEnded      = true;

    if (nPreviousCurrentMission == -1u)
        {
            RandomizedMission = nullptr;
            OriginalMission   = nullptr;
        }

    return true;
}

bool
MissionRandomizer_Flow::WasMissionPassed ()
{
    return MR::sm_Globals.g_LastPassedMission == nPreviousCurrentMission
           && MR::sm_Globals.g_LastPassedMissionTime != nLastPassedMissionTime;
}

bool
MissionRandomizer_Flow::Process (scrProgram *program, scrThreadContext *ctx)
{
    if (!MR::sm_Order.IsInitialised ())
        return true;

    HandleCurrentMissionChanges ();

    if (bCallMissionEndNextFrame && ctx->m_nState != eScriptState::KILLED)
        return OnMissionEnd (WasMissionPassed ());

    if (!RandomizedMission || !OriginalMission
        || ctx->m_nScriptHash != RandomizedMission->nHash)
        return true;

    // Defer Mission pass callback to the next valid script execution
    if (ctx->m_nState == eScriptState::KILLED)
        {
            bCallMissionEndNextFrame = true;
            return true;
        }

    // Script just started
    if (ctx->m_nIp == 0)
        return OnMissionStart ();

    return true;
}
