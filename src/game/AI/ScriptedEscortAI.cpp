/*
 * Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * Copyright (C) 2008-2014 Hellground <http://hellground.net/>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* ScriptData
SDName: Npc_EscortAI
SD%Complete: 100
SDComment:
SDCategory: Npc
EndScriptData */

#include "ScriptedPch.h"
#include "ScriptedEscortAI.h"

enum ePoints
{
    POINT_LAST_POINT    = 0xFFFFFF,
    POINT_HOME          = 0xFFFFFE
};

npc_escortAI::npc_escortAI(Creature* pCreature) : ScriptedAI(pCreature),
    PlayerGUID(0),
    MaxPlayerDistance(DEFAULT_MAX_PLAYER_DISTANCE),
    PlayerCheckTimer(1000),
    WPWaitTimer(2500),
    EscortState(STATE_ESCORT_NONE),
    IsActiveAttacker(true),
    IsRunning(false),
    DespawnAtEnd(true),
    DespawnAtFar(true),
    QuestForEscort(NULL),
    CanInstantRespawn(false),
    CanReturnToStart(false),
    ScriptWP(false),
    ClearWaypoints(false)
{}

void npc_escortAI::AttackStart(Unit* pWho)
{
    if (!pWho)
        return;

    if (me->Attack(pWho, true))
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE && !HasEscortState(STATE_ESCORT_INCOMBAT))
            AddEscortState(STATE_ESCORT_INCOMBAT);

        if (IsCombatMovement())
        {
            me->StopMoving();
            me->GetMotionMaster()->MoveChase(pWho);
        }
    }
}

//see followerAI
bool npc_escortAI::AssistPlayerInCombat(Unit* pWho)
{
    if (!pWho || !pWho->getVictim() || !IsActiveAttacker)
        return false;

    //experimental (unknown) flag not present
    if (!(me->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_UNK13))
        return false;

    //not a player
    if (!pWho->getVictim()->GetCharmerOrOwnerPlayerOrPlayerItself())
        return false;

    //never attack friendly
    if (me->IsFriendlyTo(pWho))
        return false;

    //too far away and no free sight?
    if (me->IsWithinDistInMap(pWho, GetMaxPlayerDistance()) && me->IsWithinLOSInMap(pWho))
    {
        //already fighting someone?
        if (!me->getVictim())
        {
            AttackStart(pWho);
            return true;
        }
        else
        {
            pWho->SetInCombatWith(me);
            me->AddThreat(pWho, 0.0f);
            return true;
        }
    }

    return false;
}

void npc_escortAI::MoveInLineOfSight(Unit* pWho)
{
    if (!me->hasUnitState(UNIT_STAT_STUNNED) && pWho->isTargetableForAttack() && pWho->isInAccessiblePlacefor(me))
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING) && AssistPlayerInCombat(pWho))
            return;

        if (!me->CanFly() && me->GetDistanceZ(pWho) > CREATURE_Z_ATTACK_RANGE)
            return;

        if (me->IsHostileTo(pWho) && IsActiveAttacker)
        {
            float fAttackRadius = me->GetAttackDistance(pWho);
            if (me->IsWithinDistInMap(pWho, fAttackRadius) && me->IsWithinLOSInMap(pWho))
            {
                if (!me->getVictim())
                {
                    pWho->RemoveAurasDueToSpell(SPELL_AURA_MOD_STEALTH);
                    AttackStart(pWho);
                }
                else if (me->GetMap()->IsDungeon())
                {
                    pWho->SetInCombatWith(me);
                    me->AddThreat(pWho, 0.0f);
                }
            }
        }
    }
}

void npc_escortAI::JustDied(Unit* pKiller)
{
    if (!HasEscortState(STATE_ESCORT_ESCORTING) || !PlayerGUID || !QuestForEscort)
        return;

    if (Player* pPlayer = GetPlayerForEscort())
    {
        if (Group* pGroup = pPlayer->GetGroup())
        {
            for (GroupReference* pRef = pGroup->GetFirstMember(); pRef != NULL; pRef = pRef->next())
            {
                if (Player* pMember = pRef->getSource())
                {
                    if (pMember->GetQuestStatus(QuestForEscort->GetQuestId()) == QUEST_STATUS_INCOMPLETE)
                        pMember->FailQuest(QuestForEscort->GetQuestId());
                }
            }
        }
        else
        {
            if (pPlayer->GetQuestStatus(QuestForEscort->GetQuestId()) == QUEST_STATUS_INCOMPLETE)
                pPlayer->FailQuest(QuestForEscort->GetQuestId());
        }
    }
}

void npc_escortAI::JustRespawned()
{
    EscortState = STATE_ESCORT_NONE;

    if (!IsCombatMovement())
        SetCombatMovement(true);

    //add a small delay before going to first waypoint, normal in near all cases
    WPWaitTimer = 2500;

    if (me->getFaction() != me->GetCreatureInfo()->faction_A)
        me->RestoreFaction();

    Reset();
}

void npc_escortAI::ReturnToLastPoint()
{
    float x, y, z, o;
    me->GetHomePosition(x, y, z, o);
    me->GetMotionMaster()->MovePoint(POINT_LAST_POINT, x, y, z);
}

void npc_escortAI::EnterEvadeMode()
{
    me->RemoveAllAuras();
    me->DeleteThreatList();
    me->CombatStop(true);
    me->SetLootRecipient(NULL);

    if (HasEscortState(STATE_ESCORT_ESCORTING))
    {
        ReturnToLastPoint();
    }
    else
    {
        me->GetMotionMaster()->MoveTargetedHome();

        Reset();
    } 
}

bool npc_escortAI::IsPlayerOrGroupInRange()
{
    if (Player* pPlayer = GetPlayerForEscort())
    {
        if (Group* pGroup = pPlayer->GetGroup())
        {
            for (GroupReference* pRef = pGroup->GetFirstMember(); pRef != NULL; pRef = pRef->next())
            {
                Player* pMember = pRef->getSource();

                if (pMember && me->IsWithinDistInMap(pMember, GetMaxPlayerDistance()))
                {
                    return true;
                    break;
                }
            }
        }
        else
        {
            if (me->IsWithinDistInMap(pPlayer, GetMaxPlayerDistance()))
                return true;
        }
    }
    return false;
}

void npc_escortAI::UpdateAI(const uint32 uiDiff)
{
    //Waypoint Updating
    if (HasEscortState(STATE_ESCORT_ESCORTING) && !me->isInCombat() && WPWaitTimer && !HasEscortState(STATE_ESCORT_INCOMBAT))
    {
        if (WPWaitTimer <= uiDiff)
        {
            //End of the line
            if (CurrentWP == WaypointList.end())
            {
                if (DespawnAtEnd)
                {
                    if (CanReturnToStart)
                    {
                        float fRetX, fRetY, fRetZ;
                        me->GetRespawnCoord(fRetX, fRetY, fRetZ);

                        me->GetMotionMaster()->MovePoint(POINT_HOME, fRetX, fRetY, fRetZ, UNIT_ACTION_CHASE);

                        WPWaitTimer = 0;

                        sLog.outDebug("TSCR: EscortAI are returning home to spawn location: %u, %f, %f, %f", POINT_HOME, fRetX, fRetY, fRetZ);
                        return;
                    }

                    if (CanInstantRespawn)
                    {
                        me->setDeathState(JUST_DIED);
                        me->Respawn();
                    }
                    else
                        me->ForcedDespawn();

                    return;
                }
                else
                {
                    if (ClearWaypoints)
                    {
                        WaypointList.clear();
                        RemoveEscortState(STATE_ESCORT_ESCORTING);
                        ScriptWP = false;
                    }

                    sLog.outDebug("TSCR: EscortAI reached end of waypoints with Despawn off");

                    return;
                }
            }

            if (!HasEscortState(STATE_ESCORT_PAUSED))
            {
                me->GetMotionMaster()->MovePoint(CurrentWP->id, CurrentWP->x, CurrentWP->y, CurrentWP->z);
                sLog.outDebug("TSCR: EscortAI start waypoint %u (%f, %f, %f).", CurrentWP->id, CurrentWP->x, CurrentWP->y, CurrentWP->z);

                WaypointStart(CurrentWP->id);

                WPWaitTimer = 0;
            }
        }
        else
            WPWaitTimer -= uiDiff;
    }

    //Check if player or any member of his group is within range
    if (HasEscortState(STATE_ESCORT_ESCORTING) && PlayerGUID && !me->isInCombat() && !HasEscortState(STATE_ESCORT_INCOMBAT))
    {
        if (PlayerCheckTimer < uiDiff)
        {
            if (DespawnAtFar && !IsPlayerOrGroupInRange())
            {
                sLog.outDebug("TSCR: EscortAI failed because player/group was to far away or not found");

                if (CanInstantRespawn)
                {
                    me->setDeathState(JUST_DIED);
                    me->Respawn();
                }
                else
                    me->ForcedDespawn();

                return;
            }

            PlayerCheckTimer = 1000;
        }
        else
            PlayerCheckTimer -= uiDiff;
    }

    UpdateEscortAI(uiDiff);
}

void npc_escortAI::UpdateEscortAI(const uint32 uiDiff)
{
    if (!UpdateVictim())
        return;

    DoMeleeAttackIfReady();
}

void npc_escortAI::MovementInform(uint32 uiMoveType, uint32 uiPointId)
{
    if (uiMoveType != POINT_MOTION_TYPE || !HasEscortState(STATE_ESCORT_ESCORTING) || me->isInCombat())
        return;

    //Combat start position reached, continue waypoint movement
    if (uiPointId == POINT_LAST_POINT)
    {
        sLog.outDebug("TSCR: EscortAI has returned to original position before combat");

        if (IsRunning && me->IsWalking())
            me->SetWalk(false);
        else if (!IsRunning && !me->IsWalking())
            me->SetWalk(true);

        me->GetUnitStateMgr().InitDefaults(false);
        RemoveEscortState(STATE_ESCORT_INCOMBAT);

        if (!WPWaitTimer)
            WPWaitTimer = 1;
    }
    else if (uiPointId == POINT_HOME)
    {
        sLog.outDebug("TSCR: EscortAI has returned to original home location and will continue from beginning of waypoint list.");

        CurrentWP = WaypointList.begin();
        WPWaitTimer = 1;
    }
    else
    {
        //Make sure that we are still on the right waypoint
        if (CurrentWP->id != uiPointId)
        {
            sLog.outError("TSCR ERROR: EscortAI reached waypoint out of order %u, expected %u", uiPointId, CurrentWP->id);
            return;
        }

        sLog.outDebug("TSCR: EscortAI Waypoint %u reached", CurrentWP->id);

        //Call WP function
        WaypointReached(CurrentWP->id);

        WPWaitTimer = CurrentWP->WaitTimeMs + 1;

        ++CurrentWP;
    }
}

void npc_escortAI::AddWaypoint(uint32 id, float x, float y, float z, uint32 WaitTimeMs)
{
    Escort_Waypoint t(id, x, y, z, WaitTimeMs);

    WaypointList.push_back(t);

    ScriptWP = true;
}

void npc_escortAI::FillPointMovementListForCreature()
{
    std::vector<ScriptPointMove> const &pPointsEntries = pSystemMgr.GetPointMoveList(me->GetEntry());

    if (pPointsEntries.empty())
        return;

    std::vector<ScriptPointMove>::const_iterator itr;

    for (itr = pPointsEntries.begin(); itr != pPointsEntries.end(); ++itr)
    {
        Escort_Waypoint pPoint(itr->uiPointId, itr->fX, itr->fY, itr->fZ, itr->uiWaitTime);
        WaypointList.push_back(pPoint);
    }
}

void npc_escortAI::SetRun(bool bRun)
{
    if (bRun)
    {
        if (!IsRunning)
            me->SetWalk(false);
        else
            sLog.outDebug("TSCR: EscortAI attempt to set run mode, but is already running.");
    }
    else
    {
        if (IsRunning)
            me->SetWalk(true);
        else
            sLog.outDebug("TSCR: EscortAI attempt to set walk mode, but is already walking.");
    }
    IsRunning = bRun;
}

//TODO: get rid of this many variables passed in function.
void npc_escortAI::Start(bool bIsActiveAttacker, bool bRun, uint64 uiPlayerGUID, const Quest* pQuest, bool bInstantRespawn, bool bCanLoopPath)
{
    if (me->getVictim())
    {
        sLog.outError("TSCR ERROR: EscortAI attempt to Start while in combat.");
        return;
    }

    if (HasEscortState(STATE_ESCORT_ESCORTING))
    {
        sLog.outError("TSCR: EscortAI attempt to Start while already escorting.");
        return;
    }

    if(!ScriptWP) // sd2 never adds wp in script, but tc does
    {

    if (!WaypointList.empty())
        WaypointList.clear();

    FillPointMovementListForCreature();

    }

    if (WaypointList.empty())
    {
        sLog.outErrorDb("TSCR: EscortAI Start with 0 waypoints (possible missing entry in script_waypoint for creature %u).", me->GetEntry());
        return;
    }

    //set variables
    IsActiveAttacker = bIsActiveAttacker;
    IsRunning = bRun;

    PlayerGUID = uiPlayerGUID;
    QuestForEscort = pQuest;

    CanInstantRespawn = bInstantRespawn;
    CanReturnToStart = bCanLoopPath;

    if (CanReturnToStart && CanInstantRespawn)
        sLog.outDebug("TSCR: EscortAI is set to return home after waypoint end and instant respawn at waypoint end. Creature will never despawn.");

    if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
    {
        me->GetMotionMaster()->MoveIdle();
        sLog.outDebug("TSCR: EscortAI start with WAYPOINT_MOTION_TYPE, changed to MoveIdle.");
    }

    //disable npcflags
    me->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    sLog.outDebug("TSCR: EscortAI started with %u waypoints. ActiveAttacker = %d, Run = %d, PlayerGUID = %u", WaypointList.size(), IsActiveAttacker, IsRunning, PlayerGUID);

    CurrentWP = WaypointList.begin();

    //Set initial speed
    if (IsRunning)
        me->SetWalk(false);
    else
        me->SetWalk(true);

    AddEscortState(STATE_ESCORT_ESCORTING);
}

void npc_escortAI::SetEscortPaused(bool bPaused)
{
    if (!HasEscortState(STATE_ESCORT_ESCORTING))
        return;

    if (bPaused)
        AddEscortState(STATE_ESCORT_PAUSED);
    else
        RemoveEscortState(STATE_ESCORT_PAUSED);
}
