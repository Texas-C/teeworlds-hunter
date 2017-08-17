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
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->SetHunter(false);
			GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
			GameServer()->m_apPlayers[i]->Respawn();
			GameServer()->m_apPlayers[i]->m_Score = 0;
			GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
			GameServer()->m_apPlayers[i]->m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
		}
}

void CGameControllerMOD::Tick()
{
	// this is the main part of the gamemode, this function is run every tick

	int numOfHunters = 0;
	int numOfPlayers = 0;
	int numOfSpectators = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				if(GameServer()->m_apPlayers[i]->GetHunter())
					numOfHunters++;
				else
					numOfPlayers++;
			}
			else
			{
				numOfSpectators++;
			}
		}

	if(numOfHunters == 0 && numOfPlayers == 0 && numOfSpectators > 0)
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i])
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				GameServer()->m_apPlayers[i]->SetHunter(false);
			}

	if(numOfHunters == 0 && numOfPlayers > 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i])
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				GameServer()->m_apPlayers[i]->SetHunter(false);
			}

		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "A new Hunter has been selected.");
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "To win you must figure who it is and kill them.");
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Be warned! Sudden Death.");

		int nextHunter = rand()%numOfPlayers;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
			{
				if(nextHunter == 0)
				{
					GameServer()->m_apPlayers[i]->SetHunter(true);
					break;
				}
				nextHunter--;
			}
	}

	if(numOfHunters == 1 && numOfPlayers == 0 && numOfSpectators && m_GameOverTick == -1)
	{
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "The Hunter Wins!");
		EndRound();
	}

	IGameController::Tick();
}

int CGameControllerMOD::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{

	if(pVictim && pVictim->GetPlayer() && pVictim->GetPlayer()->GetHunter())
	{
		GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
		if(pVictim->GetPlayer())
		{
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "Hunter '%s' was defeated!", Server()->ClientName(pVictim->GetPlayer()->GetCID()));
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		}
		EndRound();
	}
	else
	{
		GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
		if(pVictim->GetPlayer())
			pVictim->GetPlayer()->SetTeamDirect(TEAM_SPECTATORS);
	}

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
}

bool CGameControllerMOD::CanJoinTeam(int Team, int NotThisID)
{
	if(GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetCharacter() && !GameServer()->m_apPlayers[NotThisID]->GetCharacter()->IsAlive())
		return false;

	return IGameController::CanJoinTeam(Team,NotThisID);
}

bool CGameControllerMOD::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	return !pPlayer || !pPlayer->GetCharacter() || pPlayer->GetCharacter()->IsAlive();
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
