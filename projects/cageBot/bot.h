#ifndef BOT_H
#define BOT_H
#include "bot_interface.h"
#include "kf/kf_random.h"
#include "EnemyBot.h"
#include "node.h"

#ifdef BOT_EXPORTS
#define BOT_API __declspec(dllexport)
#else
#define BOT_API __declspec(dllimport)
#endif

class Robot : public BotInterface
{
public:
#pragma region Constructor/Initialisation
	enum BotState
	{
		ATTACK,
		SEARCHING,
		FLEE
	};

	enum TargetType
	{
		POSITION,
		ENEMY
	};

	//Main Constructor. Initialises m_rand.
	Robot();

	//Called at the start of each round. Contains map data, bot properties, and the ability to set bot attributes.
	virtual void init(const BotInitialData &initialData, BotAttributes &attrib);

	//Called at the end of each round.
	virtual void result(bool won);

	//Main Update Loop.
	virtual void update(const BotInput &input, BotOutput &output);

	BotInitialData m_initdata; //Data provided by the Killbot API, containing information related to the map, bot limitations, and bot data located in its XML config.
	bool m_started = false;	//Used for final init. Called in first update each round.
	kf::Xor128 m_rand; //Used for various operations
#pragma endregion

private:
#pragma region Behaviours / Bot States
	//Updates our bot state based on what has happened since the last update.
	void updateBotState(const BotInput &input);

	//Main Bot Logic.
	void botLogic(const BotInput &input, BotOutput &output);

	//Returns visible enemy count, and updates enemy list.
	int processScanData(const BotInput &input);

	//Gets our recommended target, otherwise a nullptr if no enemy exists, or no target is recommended.
	EnemyBot* getEnemyTarget(kf::Vector2 localPos);

	BotState m_botState; //Our current bot state.

	int m_currentHealth; //The players current health
	int m_damageTaken; //The damage the player has sustained in the current state.
	int m_lastAction; //The bots last action (shoot, scan)
	int m_tickCount; //Current Tick Count. Used for determining when the bots should perform certain actions.
	float m_lookAngle; //Our current look angle.

	std::vector<EnemyBot*> m_enemyBots; //Contains all enemies we have encountered so far.
	EnemyBot* m_enemyBot; //Our current recommended target

	//Tweakables
	//General Random Movement
	const float m_switchDist = 2.0f; //Distance until the bot picks a new target.
	const float m_newTargetDist = 15.0f; //Distance required for (random) new target.

	//Searching
	const int m_searchAttackTick = 10; //Every x ticks, attack an enemy at their last known location.

	//Fleeing
	const int m_fleeDmg = 100; //Damage to bot required to start fleeing.
	const float m_fleeDist = 7.0f; //How far we have to be away before going back into search/attack mode.
	const int m_fleeSwitchTick = 60; //When fleeing, every x ticks, switch to a new random location.
	const int m_fleeAttackTick = 4; //When fleeing, every x ticks, do an attack.

	//Attacking
	const int m_attackScanTick = 5; //When attacking, every x ticks, perform a scan.
	const int m_attackLastScanTick = 2; //When attacking, event x ticks, scan the enemies last know location.
	const int m_enemyLostTickLimit = 30; //When attacking, if an enemies location is unknown for x ticks, revert to the search state.
	const int m_attackSwitchTick = 18; //When moving away from target, at x ticks try and change target again to increase distance.
	const float m_attackSwitchDist = 5.0f; //When attacking, distance until the bot moves away from target.
#pragma endregion

#pragma region Pathfinding & Navigation
	//Calculates the cost of a node, based on its adjacent nodes.
	int calculateAdjacentCost(Node* node);
	//Calculates a path to reach our target positon.
	void calculateGoal(const BotInput &input, BotOutput &output);
	//Generates a random new Vector 2 position, and sets it as our new target position.
	void findNewPosition(kf::Vector2 startPos);

	std::vector<Node*> m_mapNodes; //All pathfinding nodes on the map.
	kf::Vector2 m_moveTarget; //The current pathfinding goal.
	TargetType m_targetType; //The type of our current target.

	Node* m_targetNode; //The node containing our pathfinding goal.
	Node* m_currentNode; //The node containing our bots current position.
	kf::Vector2 m_nextPos; //Next position for the bot to travel to.

	//Tweakables
	const int m_adjWallCost = 40; //The cost of nodes that are adjacent to a wall.
#pragma endregion

#pragma region Utils
	//Returns the (float) distance between 2 Vector2s
	float distance(kf::Vector2 pos1, kf::Vector2 pos2);
#pragma endregion
};


#endif