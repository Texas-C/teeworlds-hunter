/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/*********************************\
*                                 *
* HUNTER SERVER MOD               *
*                                 *
* By Bluelightzero                *
*                                 *
* http://github.com/bluelightzero *
*                                 *
\*********************************/

#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>

#include "mod.h"

CGameControllerMOD::CGameControllerMOD(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	// Exchange this to a string that identifies your game mode.
	// DM, TDM and CTF are reserved for teeworlds original modes.
	m_pGameType = "hunter";

	//m_GameFlags = GAMEFLAG_TEAMS; // GAMEFLAG_TEAMS makes it a two-team gamemode
}

void CGameControllerMOD::PostReset()
{
	m_Hunters = 0;
	m_Civics = 0;
	m_Deathes = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->SetHunter(false);
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_WantTeam != TEAM_SPECTATORS)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(GameServer()->m_apPlayers[i]->m_WantTeam));
				GameServer()->m_apPlayers[i]->Respawn();
				GameServer()->m_apPlayers[i]->m_Score = 0;
				GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
				GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
			}
			if(GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_Civics++;
		}
}

void CGameControllerMOD::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	if(!GameServer()->m_World.m_Paused && !IsGameOver())
	{
		char aBuf[128];

		// when there are no hunters, try to elect one
		// sleep when no one play
		if(m_Hunters == 0 && (m_Civics || Server()->Tick() % (Server()->TickSpeed() * 3) == 0))
		{
			// release who wants to join the game first
			PostReset();

			// elect hunter
			if(m_Civics <= 1)
			{
				if(Server()->Tick() % Server()->TickSpeed() == 0)
					GameServer()->SendBroadcast("At least 2 players to start game", -1);
			}
			else
			{
				// clear broadcast
				GameServer()->SendBroadcast("", -1);

				m_Hunters = (m_Civics + 4) / 5;

				for(int iHunter = 0; iHunter < m_Hunters; iHunter++)
				{
					int nextHunter = rand() % m_Civics;
					for(int i = 0; i < MAX_CLIENTS; i++)
						// only players get elected
						if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GameServer()->m_apPlayers[i]->GetHunter())
						{
							if(nextHunter == 0)
							{
								m_Civics--;
								GameServer()->m_apPlayers[i]->SetHunter(true);
								str_format(aBuf, sizeof(aBuf), "'%d:%s' is hunter", GameServer()->m_apPlayers[i]->GetCID(), Server()->ClientName(GameServer()->m_apPlayers[i]->GetCID()));
								GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
								break;
							}
							nextHunter--;
						}
				}

				// notify all
				if(m_Hunters == 1)
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "A new Hunter has been selected.");
				else
				{
					str_format(aBuf, sizeof(aBuf), "%d new Hunters have been selected.", m_Hunters);
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
				}
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "To win you must figure who it is and kill them.");
				GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Be warned! Sudden Death.");
			}
		}
	}

	IGameController::Tick();
}

int CGameControllerMOD::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(pVictim->GetPlayer()->GetHunter())
	{
		m_Hunters--;
		if(m_Hunters == 0 && m_Civics)
		{
			GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Hunter '%s' was defeated!", Server()->ClientName(pVictim->GetPlayer()->GetCID()));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
			EndRound();
			return 0;
		}
	}
	else
	{
		m_Civics--;
		m_Deathes++;
	}
	GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
	pVictim->GetPlayer()->SetTeamDirect(TEAM_SPECTATORS);

	/*
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;

	if(Weapon == WEAPON_SELF)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()*3.0f;
		*/
	return 0;
}

void CGameControllerMOD::DoWincheck()
{
	if(m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		// when only hunter left, he wins
		if(m_Hunters && m_Civics == 0 && m_Deathes)
		{
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "The Hunter(s) Wins!");
			m_Deathes = 0;
			EndRound();
		}
	}
}

bool CGameControllerMOD::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", "CanChangeTeam");
	if(JoinTeam == TEAM_SPECTATORS)
		return true;

	if(m_Hunters == 0)
		return true;

	return pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive();
}

void CGameControllerMOD::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	if(pChr->GetPlayer()->GetHunter())
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GUN, 10);
}
