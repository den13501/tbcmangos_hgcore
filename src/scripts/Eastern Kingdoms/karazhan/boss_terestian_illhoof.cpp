/* 
 * Copyright (C) 2006-2008 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
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
SDName: Boss_Terestian_Illhoof
SD%Complete: 95
SDComment: Complete! Needs adjustments to use spell though.
SDCategory: Karazhan
EndScriptData */

#include "ScriptedPch.h"
#include "def_karazhan.h"

#define SAY_SLAY1                   -1532065
#define SAY_SLAY2                   -1532066
#define SAY_DEATH                   -1532067
#define SAY_AGGRO                   -1532068
#define SAY_SACRIFICE1              -1532069
#define SAY_SACRIFICE2              -1532070
#define SAY_SUMMON1                 -1532071
#define SAY_SUMMON2                 -1532072

#define SPELL_SUMMON_DEMONCHAINS    30120                   // Summons demonic chains that maintain the ritual of sacrifice.
#define SPELL_DEMON_CHAINS          30206                   // Instant - Visual Effect
#define SPELL_ENRAGE                23537                   // Increases the caster's attack speed by 50% and the Physical damage it deals by 219 to 281 for 10 min.
#define SPELL_SHADOW_BOLT           30055                   // Hurls a bolt of dark magic at an enemy, inflicting Shadow damage.
#define SPELL_SACRIFICE             30115                   // Teleports and adds the debuff
#define SPELL_BERSERK               32965                   // Increases attack speed by 75%. Periodically casts Shadow Bolt Volley.

#define SPELL_SUMMON_IMP            30066                   // Summons Kil'rek

#define SPELL_FIENDISH_PORTAL       30171                   // Opens portal and summons Fiendish Portal, 2 sec cast
#define SPELL_FIENDISH_PORTAL_1     30179                   // Opens portal and summons Fiendish Portal, instant cast

#define SPELL_BROKEN_PACT           30065                   // All damage taken increased by 25%.
#define SPELL_AMPLIFY_FLAMES        30053                   // Increases the Fire damage taken by an enemy by 500 for 25 sec.
#define SPELL_FIREBOLT              30050                   // Blasts a target for 150 Fire damage.

#define CREATURE_DEMONCHAINS    17248
#define CREATURE_FIENDISHIMP    17267
#define CREATURE_PORTAL         17265
#define CREATURE_KILREK            17229

#define KILREK_POS_X    -1.7001953125
#define KILREK_POS_Y     3.949951171875
#define KILREK_POS_Z     0.001007080078125

#define PORTAL_Z        179.434

float PortalLocations[2][2]=
{
    {-11249.6933, -1704.61023},
    {-11242.1160, -1713.33325},
};

struct mob_kilrekAI : public ScriptedAI
{
    mob_kilrekAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = (c->GetInstanceData());
    }

    ScriptedInstance* pInstance;

    uint64 TerestianGUID;

    uint32 AmplifyTimer;

    void Reset()
    {
        TerestianGUID = 0;

        AmplifyTimer = 2000;
    }

    void EnterCombat(Unit *who)
    {
        if(!pInstance)
        {
            ERROR_INST_DATA(m_creature);
            return;
        }

        Creature* Terestian = (Unit::GetCreature(*m_creature, pInstance->GetData64(DATA_TERESTIAN)));
        if(Terestian && !Terestian->getVictim())
            Terestian->AddThreat(who, 1.0f);
    }

    void JustDied(Unit* Killer)
    {
        if(pInstance)
        {
            uint64 TerestianGUID = pInstance->GetData64(DATA_TERESTIAN);
            if(TerestianGUID)
            {
                Unit* Terestian = Unit::GetUnit((*m_creature), TerestianGUID);
                if(Terestian && Terestian->isAlive())
                    DoCast(Terestian, SPELL_BROKEN_PACT, true);
            }
        }else ERROR_INST_DATA(m_creature);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim() )
            return;

        if (AmplifyTimer < diff)
        {
            m_creature->InterruptNonMeleeSpells(false);
            DoCast(m_creature->getVictim(),SPELL_AMPLIFY_FLAMES);

            AmplifyTimer = 10000 + rand()%10000;
        }else AmplifyTimer -= diff;

        //Chain cast
        if (!m_creature->IsNonMeleeSpellCast(false) && m_creature->IsWithinDistInMap(m_creature->getVictim(), 30))
            DoCast(m_creature->getVictim(),SPELL_FIREBOLT);
        else DoMeleeAttackIfReady();
    }
};

struct mob_demon_chainAI : public Scripted_NoMovementAI
{
    mob_demon_chainAI(Creature *c) : Scripted_NoMovementAI(c) {}

    uint64 SacrificeGUID;

    void Reset()
    {
        SacrificeGUID = 0;
    }

    void AttackStart(Unit* who) {}
    void MoveInLineOfSight(Unit* who) {}

    void JustDied(Unit *killer)
    {
        if(SacrificeGUID)
        {
            Unit* Sacrifice = Unit::GetUnit((*m_creature),SacrificeGUID);
            if(Sacrifice)
                Sacrifice->RemoveAurasDueToSpell(SPELL_SACRIFICE);
        }
    }
};

struct boss_terestianAI : public ScriptedAI
{
    boss_terestianAI(Creature *c) : ScriptedAI(c)
    {
        pInstance = (c->GetInstanceData());
        m_creature->GetPosition(wLoc);
    }

    ScriptedInstance *pInstance;

    uint64 PortalGUID[2];

    uint32 SacrificeTimer;
    uint32 ShadowboltTimer;
    uint32 SummonTimer;
    uint32 BerserkTimer;
    uint32 CheckTimer;

    WorldLocation wLoc;

    bool SummonedPortals;
    bool Berserk;

    void Reset()
    {
        for(uint8 i = 0; i < 2; ++i)
        {
            if(PortalGUID[i])
            {
                Unit* Portal = Unit::GetUnit((*m_creature), PortalGUID[i]);
                if(Portal)
                    Portal->DealDamage(Portal, Portal->GetHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);

                PortalGUID[i] = 0;
            }
        }

        SacrificeTimer      = 30000;
        CheckTimer          = 3000;
        ShadowboltTimer     =  5000;
        SummonTimer         = 10000;
        BerserkTimer        = 600000;

        SummonedPortals     = false;
        Berserk             = false;

        if(pInstance && pInstance->GetData(DATA_TERESTIAN_EVENT) != DONE)
            pInstance->SetData(DATA_TERESTIAN_EVENT, NOT_STARTED);
    }

    void EnterEvadeMode()
    {
        if (Creature * kilrek = me->GetCreature(pInstance->GetData64(DATA_KILREK)))
        {
            if (kilrek->isAlive())
                kilrek->AI()->EnterEvadeMode();
            else
                kilrek->Respawn();
        }

        ScriptedAI::EnterEvadeMode();
    }

    void EnterCombat(Unit* who)
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if(pInstance)
        {
            // Put Kil'rek in combat against our target so players don't skip him
            Creature* Kilrek = (Unit::GetCreature(*m_creature, pInstance->GetData64(DATA_KILREK)));
            if(Kilrek && !Kilrek->getVictim())
                Kilrek->AddThreat(who, 1.0f);

            pInstance->SetData(DATA_TERESTIAN_EVENT, IN_PROGRESS);
        }
        else
            ERROR_INST_DATA(m_creature);
    }

    void KilledUnit(Unit *victim)
    {
        DoScriptText(RAND(SAY_SLAY1, SAY_SLAY2), m_creature);
    }

    void JustDied(Unit *killer)
    {
        for(uint8 i = 0; i < 2; ++i)
        {
            if(PortalGUID[i])
            {
                Unit* Portal = Unit::GetUnit((*m_creature), PortalGUID[i]);
                if(Portal)
                    Portal->DealDamage(Portal, Portal->GetHealth(), DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);

                PortalGUID[i] = 0;
            }
        }

        DoScriptText(SAY_DEATH, m_creature);

        if(pInstance)
            pInstance->SetData(DATA_TERESTIAN_EVENT, DONE);
    }

    void UpdateAI(const uint32 diff)
    {
        if(!UpdateVictim())
            return;

        //Check_Timer
        if(CheckTimer < diff)
        {
            if(!m_creature->IsWithinDistInMap(&wLoc, 35.0f))
                EnterEvadeMode();
            else
                DoZoneInCombat();

            CheckTimer = 3000;
        }
        else
            CheckTimer -= diff;

        if(SacrificeTimer < diff)
        {
            Unit* target = SelectUnit(SELECT_TARGET_RANDOM, 1, GetSpellMaxRange(SPELL_SACRIFICE), true, m_creature->getVictimGUID());
            if(target && target->isAlive() && target->GetTypeId() == TYPEID_PLAYER)
            {
                DoCast(target, SPELL_SACRIFICE, true);
                Creature* Chains = m_creature->SummonCreature(CREATURE_DEMONCHAINS, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 21000);
                if(Chains)
                {
                    ((mob_demon_chainAI*)Chains->AI())->SacrificeGUID = target->GetGUID();
                    Chains->CastSpell(Chains, SPELL_DEMON_CHAINS, true);

                    DoScriptText(RAND(SAY_SACRIFICE1, SAY_SACRIFICE2), m_creature);

                    SacrificeTimer = 30000;
                }
            }
        }
        else
            SacrificeTimer -= diff;

        if(ShadowboltTimer < diff)
        {
            if(Unit *target = SelectUnit(SELECT_TARGET_TOPAGGRO,0, GetSpellMaxRange(SPELL_SHADOW_BOLT), true))
                DoCast(target, SPELL_SHADOW_BOLT);

            ShadowboltTimer = 10000;
        }
        else
            ShadowboltTimer -= diff;

        if(SummonTimer < diff)
        {
            if(!SummonedPortals)
            {
                for(uint8 i = 0; i < 2; ++i)
                {
                    Creature* Portal = m_creature->SummonCreature(CREATURE_PORTAL, PortalLocations[i][0], PortalLocations[i][1], PORTAL_Z, 0, TEMPSUMMON_CORPSE_DESPAWN, 0);
                    if(Portal)
                        PortalGUID[i] = Portal->GetGUID();
                }
                SummonedPortals = true;

                DoScriptText(RAND(SAY_SUMMON1, SAY_SUMMON2), m_creature);
            }

            uint32 random = rand()%2;
            Creature* Imp = m_creature->SummonCreature(CREATURE_FIENDISHIMP, PortalLocations[random][0], PortalLocations[random][1], PORTAL_Z, 0, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 15000);

            if(Imp)
            {
                Imp->AddThreat(m_creature->getVictim(), 1.0f);
                if(Unit *target = SelectUnit(SELECT_TARGET_RANDOM, 1, 200, true, m_creature->getVictimGUID()))
                    Imp->AI()->AttackStart(target);
            }
            SummonTimer = 5000;
        }
        else
            SummonTimer -= diff;

        if(!Berserk)
        {
            if(BerserkTimer < diff)
            {
                DoCast(m_creature, SPELL_BERSERK);
                Berserk = true;
            }
            else
                BerserkTimer -= diff;
        }

        DoMeleeAttackIfReady();
    }
};

struct mob_fiendish_impAI : public ScriptedAI
{
    mob_fiendish_impAI(Creature *c) : ScriptedAI(c) {}

    uint32 FireboltTimer;

    void Reset()
    {
        FireboltTimer = 2000;
    }

    void EnterCombat(Unit *who) {}

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim() )
            return;

        if(FireboltTimer < diff)
        {
            DoCast(m_creature->getVictim(), SPELL_FIREBOLT);
            FireboltTimer = 2200;
        }
        else
            FireboltTimer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_mob_kilrek(Creature *_Creature)
{
    return new mob_kilrekAI (_Creature);
}

CreatureAI* GetAI_mob_fiendish_imp(Creature *_Creature)
{
    return new mob_fiendish_impAI (_Creature);
}

CreatureAI* GetAI_mob_demon_chain(Creature *_Creature)
{
    return new mob_demon_chainAI(_Creature);
}

CreatureAI* GetAI_boss_terestian_illhoof(Creature *_Creature)
{
    return new boss_terestianAI (_Creature);
}

void AddSC_boss_terestian_illhoof()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="boss_terestian_illhoof";
    newscript->GetAI = &GetAI_boss_terestian_illhoof;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="mob_fiendish_imp";
    newscript->GetAI = &GetAI_mob_fiendish_imp;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name="mob_kilrek";
    newscript->GetAI = &GetAI_mob_kilrek;
    newscript->RegisterSelf();

    newscript = new Script;
    newscript->Name = "mob_demon_chain";
    newscript->GetAI = &GetAI_mob_demon_chain;
    newscript->RegisterSelf();
}

