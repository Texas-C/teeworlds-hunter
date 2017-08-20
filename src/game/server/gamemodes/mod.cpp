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

#include <engine/shared/config.h>
#include <game/server/player.h>

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
	m_HunterDeathes = 0;
	m_Civics = 0;
	m_CivicDeathes = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->SetHunter(false);
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS && GameServer()->m_apPlayers[i]->m_WantTeam != TEAM_SPECTATORS)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(GameServer()->m_apPlayers[i]->m_WantTeam));
				GameServer()->m_apPlayers[i]->Respawn();
				//GameServer()->m_apPlayers[i]->m_Score = 0;
				//GameServer()->m_apPlayers[i]->m_ScoreStartTick = Server()->Tick();
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
		char aBuf[512];

		// when there are no hunters, try to elect one
		// sleep when no one play
		if(m_Hunters == 0 && (m_Civics || Server()->Tick() % (Server()->TickSpeed() * 3) == 0))
		{
			bool SoloPlayerBefore = m_Civics == 1;

			// release who wants to join the game first
			PostReset();

			// elect hunter
			int LeastPlayers = g_Config.m_HuntFixedHunter ? (g_Config.m_HuntHunterNumber + 1) : 2;
			if(m_Civics < LeastPlayers)
			{
				if(Server()->Tick() % (Server()->TickSpeed() * 2) == 0)
				{
					str_format(aBuf, sizeof(aBuf), "At least %d players to start game", LeastPlayers);
					GameServer()->SendBroadcast(aBuf, -1);
					if(g_Config.m_SvTimelimit > 0)
						m_RoundStartTick = Server()->Tick();
				}
			}
			else if(SoloPlayerBefore)
			{
				// clear broadcast
				GameServer()->SendBroadcast("", -1);
				StartRound();
			}
			else
			{
				m_Hunters = g_Config.m_HuntFixedHunter ? g_Config.m_HuntHunterNumber : ((m_Civics + g_Config.m_HuntHunterRatio - 1) / g_Config.m_HuntHunterRatio);
				if(m_Hunters == 1)
				{
					str_copy(aBuf, "Hunter is: ", sizeof(aBuf));
					str_copy(m_aHuntersMessage, "Hunter is: ", sizeof(m_aHuntersMessage));
				}
				else
				{
					str_copy(aBuf, "Hunters are: ", sizeof(aBuf));
					str_copy(m_aHuntersMessage, "Hunters are: ", sizeof(m_aHuntersMessage));
				}

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

								// give him hammer in case of infinite mode
								if(GameServer()->m_apPlayers[i]->GetCharacter())
									GameServer()->m_apPlayers[i]->GetCharacter()->GiveWeapon(WEAPON_HAMMER, -1);

								// generate info message
								const char* ClientName = Server()->ClientName(GameServer()->m_apPlayers[i]->GetCID());
								str_append(m_aHuntersMessage, ClientName, sizeof(m_aHuntersMessage));
								if(m_Hunters - iHunter > 1)
									str_append(m_aHuntersMessage, ", ", sizeof(m_aHuntersMessage));
								int aBufLen = str_length(aBuf);
								str_format(aBuf + aBufLen, sizeof(aBuf) - aBufLen, m_Hunters - iHunter == 1 ? "%d:%s" : "%d:%s, ", GameServer()->m_apPlayers[i]->GetCID(), ClientName);
								break;
							}
							nextHunter--;
						}
				}
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

				// notify all
				GameServer()->SendChatTarget(-1, "============");
				if(m_Hunters == 1)
					GameServer()->SendChatTarget(-1, "A new Hunter has been selected.");
				else
				{
					str_format(aBuf, sizeof(aBuf), "%d new Hunters have been selected.", m_Hunters);
					GameServer()->SendChatTarget(-1, aBuf);
				}
				GameServer()->SendChatTarget(-1, "To win you must figure who it is and kill them.");
				GameServer()->SendChatTarget(-1, "Be warned! Sudden Death.");

				for(int i = 0; i < MAX_CLIENTS; i++)
					if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
						GameServer()->SendChatTarget(GameServer()->m_apPlayers[i]->GetCID(), m_aHuntersMessage);
			}
		}
	}

	IGameController::Tick();
}

int CGameControllerMOD::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	// game not start
	if(m_Hunters + m_HunterDeathes == 0)
		return 0;

	char aBuf[128];

	if(pVictim->GetPlayer()->GetHunter())
	{
		m_Hunters--;
		m_HunterDeathes++;
		// send message notify
		str_format(aBuf, sizeof(aBuf), "Hunter '%s' was defeated!", Server()->ClientName(pVictim->GetPlayer()->GetCID()));
		if(g_Config.m_HuntBroadcastHunterDeath || m_Hunters == 0)
			GameServer()->SendChatTarget(-1, aBuf);
		else
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[i]->GetHunter()))
						GameServer()->SendChatTarget(GameServer()->m_apPlayers[i]->GetCID(), aBuf);
		// add score
		if(!pKiller->GetHunter())
			pKiller->m_HiddenScore++;
		// if the last hunter, leave DoWincheck() to play the sound
		if(m_Hunters)
		{
			if(g_Config.m_HuntBroadcastHunterDeath)
				GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
			else
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(GameServer()->m_apPlayers[i])
					{
						if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[i]->GetHunter())
							GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE, GameServer()->m_apPlayers[i]->GetCID());
						else
							GameServer()->CreateSoundGlobal(SOUND_CTF_DROP, GameServer()->m_apPlayers[i]->GetCID());
					}
		}
		else
			DoWincheck();
	}
	else
	{
		m_Civics--;
		m_CivicDeathes++;
		GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
	}

	// revel hunters (except for the only hunter or the last civic)
	if(!(pVictim->GetPlayer()->GetHunter() ? m_Hunters + m_HunterDeathes == 1 : m_Civics == 1))
		GameServer()->SendChatTarget(pVictim->GetPlayer()->GetCID(), m_aHuntersMessage);
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
	const char *pWinMessage;

	char aBuf[128];

	if(m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		if(m_Hunters < 0 || m_Civics < 0 || m_CivicDeathes < 0 || m_HunterDeathes < 0 || (m_Hunters && m_Civics == 0 && m_CivicDeathes <= 0))
		{
			str_format(aBuf, sizeof(aBuf), "BUG! m_Hunters %d m_Civics %d m_CivicDeathes %d m_HunterDeathes %d", m_Hunters, m_Civics, m_CivicDeathes, m_HunterDeathes);
			pWinMessage = aBuf;
		}
		// only hunters left
		else if(m_Hunters && m_Civics == 0)
		{
			if(m_Hunters + m_HunterDeathes == 1)
				pWinMessage = "The Hunter Wins!";
			else
				pWinMessage = "The Hunters Win!";
			// add score for live hunters
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetHunter() && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
					GameServer()->m_apPlayers[i]->m_HiddenScore += 3;
		}
		// timeout or hunters died out
		else if((g_Config.m_SvTimelimit > 0 && (Server()->Tick()-m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed()*60) || (m_Hunters == 0 && m_Civics && m_HunterDeathes))
			pWinMessage = "Civics Survived!";
		// no one left
		else if(m_Hunters == 0 && m_Civics == 0 && m_HunterDeathes)
			pWinMessage = "Tie!";
		else
			goto no_win;

		GameServer()->SendChatTarget(-1, pWinMessage);
		// round end sound
		GameServer()->CreateSoundGlobal(SOUND_CTF_CAPTURE);
		// add hidden scores for players
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i])
			{
				// add players back to team to show the score
				if(GameServer()->m_apPlayers[i]->m_WantTeam != TEAM_SPECTATORS)
					GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));
				GameServer()->m_apPlayers[i]->m_Score += GameServer()->m_apPlayers[i]->m_HiddenScore;
				GameServer()->m_apPlayers[i]->m_HiddenScore = 0;
			}
		EndRound();

		no_win:;
	}
}

bool CGameControllerMOD::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
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
