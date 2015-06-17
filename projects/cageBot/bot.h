#ifndef BOT_H
#define BOT_H
#include "bot_interface.h"
#include "kf/kf_random.h"
#include "EnemyBot.h"

#ifdef BOT_EXPORTS
#define BOT_API __declspec(dllexport)
#else
#define BOT_API __declspec(dllimport)
#endif

enum BotState
{
	ATTACK,
	SEARCHING,
	FLEE
};

class Robot :public BotInterface
{
public:
	Robot();
	virtual void init(const BotInitialData &initialData, BotAttributes &attrib);
	virtual void update(const BotInput &input, BotOutput &output);
	virtual void result(bool won);

	void changeState(BotState state);
	void findNewPosition(kf::Vector2 startPos);

	EnemyBot* getEnemyTarget(kf::Vector2 localPos);

	kf::Xor128 m_rand;

	//Init Data
	BotInitialData m_initdata;

	//Bot State
	BotState m_botState;

	//Action
	int m_lastAction;

	//State Data
	int m_tickCount;
	kf::Vector2 m_moveTarget;
	float m_lookAngle;

	//Player Attributes
	int m_currentHealth;

	//Enemies
	std::vector<EnemyBot> m_enemyBots;

private:
	//Tweakables
	//General Random Movement
	const float m_switchDist = 2.0f; //Distance until the bot picks a new target.
	const float m_newTargetDist = 15.0f; //Distance required for (random) new target.

	//Ticks
		//Searching
	const int m_searchAttackTick = 10; //Every x ticks, attack an enemy at their last known location.

		//Fleeing
	const float m_fleeDist = 10.0f; //How far we have to be away before going back into search/attack mode.
	const int m_fleeSwitchTick = 6; //When fleeing, every x ticks, switch to a new random location.
	const int m_fleeAttackTick = 4; //When fleeing, every x ticks, do an attack.

		//Attacking
	const int m_attackScanTick = 5; //When attacking, every x ticks, perform a scan.
	const int m_attackLastScanTick = 2;
	const int m_attackSwitchTick = 20; //When attacking, every x ticks, switch to a new random location.
	const int m_enemyLostTickLimit = 10; //When attacking, if an enemies location is unknown for x ticks, revert to the search state.
	const int m_rushTick = 10;
	const float m_maxAttackDist = 5.0f;
	const float m_minAttackDist = 5.0f;
};


#endif