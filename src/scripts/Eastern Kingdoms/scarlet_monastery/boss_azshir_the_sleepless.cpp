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
SDName: Boss_Azshir_the_Sleepless
SD%Complete: 80
SDComment:
SDCategory: Scarlet Monastery
EndScriptData */

#include "ScriptedPch.h"

#define SPELL_CALLOFTHEGRAVE            17831
#define SPELL_TERRIFY                   7399
#define SPELL_SOULSIPHON                7290

struct boss_azshir_the_sleeplessAI : public ScriptedAI
{
    boss_azshir_the_sleeplessAI(Creature *c) : ScriptedAI(c) {}

    uint32 SoulSiphon_Timer;
    uint32 CallOftheGrave_Timer;
    uint32 Terrify_Timer;

    void Reset()
    {
        SoulSiphon_Timer = 1;
        CallOftheGrave_Timer = 30000;
        Terrify_Timer = 20000;
    }

    void EnterCombat(Unit *who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        //If we are <50% hp cast Soul Siphon rank 1
        if ( m_creature->GetHealth()*100 / m_creature->GetMaxHealth() <= 50 && !m_creature->IsNonMeleeSpellCast(false))
        {
            //SoulSiphon_Timer
            if (SoulSiphon_Timer < diff)
            {
                DoCast(m_creature->getVictim(),SPELL_SOULSIPHON);
                return;

                SoulSiphon_Timer = 20000;
            }else SoulSiphon_Timer -= diff;
        }

        //CallOfTheGrave_Timer
        if (CallOftheGrave_Timer < diff)
        {
            DoCast(m_creature->getVictim(),SPELL_CALLOFTHEGRAVE);
            CallOftheGrave_Timer = 30000;
        }else CallOftheGrave_Timer -= diff;

        //Terrify_Timer
        if (Terrify_Timer < diff)
        {
            DoCast(m_creature->getVictim(),SPELL_TERRIFY);
            Terrify_Timer = 20000;
        }else Terrify_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_azshir_the_sleepless(Creature *_Creature)
{
    return new boss_azshir_the_sleeplessAI (_Creature);
}

void AddSC_boss_azshir_the_sleepless()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="boss_azshir_the_sleepless";
    newscript->GetAI = &GetAI_boss_azshir_the_sleepless;
    newscript->RegisterSelf();
}

