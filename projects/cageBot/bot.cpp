#include "bot.h"
#include "time.h"

extern "C"
{
	BotInterface BOT_API *CreateBot()
	{
		return new Robot();
	}
}

Robot::Robot()
{
	m_rand(time(0) + (int)this);
}

float distance(kf::Vector2 pos1, kf::Vector2 pos2)
{
	float diffY = pos1.y - pos2.y;
	float diffX = pos1.x - pos2.x;
	return sqrt((diffY * diffY) + (diffX * diffX));
}

void Robot::findNewPosition(kf::Vector2 startPos)
{
	do
	{
		m_moveTarget.set(m_rand() % (m_initdata.mapData.width - 2) + 2, m_rand() % (m_initdata.mapData.height - 2) + 2);
	}
	while (m_initdata.mapData.data[int(m_moveTarget.x) + int(m_moveTarget.y)*m_initdata.mapData.width].wall || distance(startPos, m_moveTarget) < m_newTargetDist);
}

void Robot::init(const BotInitialData &initialData, BotAttributes &attrib)
{
	m_initdata = initialData;
	attrib.health = 1.0;
	attrib.motor = 1.0;
	attrib.weaponSpeed = 1.0;
	attrib.weaponStrength = 1.0;

	findNewPosition(kf::Vector2(0.0f, 0.0f));

	m_enemyBots.clear();
}

void Robot::changeState(BotState state)
{
	if (state != m_botState)
	{
		m_botState = state;
		//This is a new state. Reset our state data
		m_tickCount = 0;
	}
}

EnemyBot* Robot::getEnemyTarget(kf::Vector2 localPos)
{
	EnemyBot* targetBot = nullptr;
	float targetDist = 0.0f;

	for (EnemyBot &bot : m_enemyBots)
	{
		if (bot.m_invisible < m_enemyLostTickLimit)
		{
			float dist = bot.getDistanceToPostion(localPos);
			if (dist < targetDist || targetDist == 0.0f)
			{
				targetBot = &bot;
				targetDist = dist;
			}
		}
	}

	return targetBot;
}

void Robot::update(const BotInput &input, BotOutput &output)
{
	//Reset Data
	output.lines.clear();
	int visibleEnemyCount = 0;

	//Process Scan Results
	if (m_lastAction == BotOutput::scan)
	{
		for (auto scanResult : input.scanResult)
		{
			if (scanResult.type == VisibleThing::e_robot)
			{
				EnemyBot* bot = nullptr;

				for (EnemyBot &knownBot : m_enemyBots)
				{
					if (knownBot.m_botID == scanResult.name)
					{
						bot = &knownBot;
						break;
					}
				}

				if (bot == nullptr)
				{
					m_enemyBots.push_back(EnemyBot(scanResult.name));
					bot = &m_enemyBots.back();
				}

				bot->setPos(scanResult.position);
				
				++visibleEnemyCount;
			}
		}
	}

	//Check current world/player states 
	EnemyBot* enemyBot = getEnemyTarget(input.position);

	if (input.health < m_currentHealth || (m_botState == FLEE && enemyBot != nullptr && enemyBot->getDistanceToPostion(input.position) < m_fleeDist))
	{
		//We Got hit, or are currently fleeing.
		changeState(FLEE);
	}
	else if (enemyBot != nullptr)
	{
		//We can see an enemy. Enter Combat Mode.
		changeState(ATTACK);
	}
	else
	{
		//Search for the enemy.
		changeState(SEARCHING);
	}

	//Increment Tick Count
	++m_tickCount;

	//Update player stats
	m_currentHealth = input.health;

	//Do Bot Logic
	switch (m_botState)
	{
		case FLEE:
		{
			//Switch move direction every x tick, or when we arrive at our target.
			if (m_tickCount % m_fleeSwitchTick == 0 || distance(m_moveTarget, input.position) < m_switchDist)
			{
				findNewPosition(input.position);
			}

			//Look back at our last known enemy location, and attack every x ticks.
			if (m_tickCount % m_fleeAttackTick == 0 && enemyBot != nullptr)
			{
				output.lookDirection = enemyBot->getPos() - input.position;
				output.action = BotOutput::shoot;
			}

			//Do the normal search sweep.
			else
			{
				output.lookDirection.set(cos(m_lookAngle), sin(m_lookAngle));
				m_lookAngle += m_initdata.scanFOV * 2;

				output.action = BotOutput::scan;
			}

			break;
		}
		case SEARCHING:
		{
			//Randomize search location on map
			//TODO Pathfinding
			//Switch Direction when we arrive at our Target

			if (distance(m_moveTarget, input.position) < m_switchDist)
			{
				findNewPosition(input.position);
			}

			//Do a normal search
			{
				output.lookDirection.set(cos(m_lookAngle), sin(m_lookAngle));
				m_lookAngle += m_initdata.scanFOV * 2;

				output.action = BotOutput::scan;
			}

			break;
		}
		case ATTACK:
		{
			float targetDist = enemyBot->getDistanceToPostion(input.position);

			//Depending on our distance, set our move target
			if (m_tickCount % m_rushTick == 0)
			{
				if (targetDist > m_maxAttackDist)
				{
					m_moveTarget = enemyBot->getPos();
				}
				else if (targetDist < m_minAttackDist)
				{
					m_moveTarget = (input.position - enemyBot->getPos()) + input.position;
				}
			}
			else
			{
				//Switch move direction every x tick, or when we arrive at our target.
				if (m_tickCount % m_attackSwitchTick == 0 || distance(m_moveTarget, input.position) < m_switchDist)
				{
					findNewPosition(input.position);
				}
			}

			//Scan every x tick
			if (m_tickCount % m_attackScanTick == 0)
			{
				output.lookDirection.set(cos(m_lookAngle), sin(m_lookAngle));
				m_lookAngle += m_initdata.scanFOV * 2;
				output.action = BotOutput::scan;
			}

			//Scan last known location of enemy every x ticks
			if (m_tickCount % m_attackLastScanTick == 0)
			{
				output.lookDirection = enemyBot->getExpectedPosition(m_tickCount % m_attackScanTick, targetDist) - input.position;
				output.action = BotOutput::scan;
			}

			//Shoot every other tick
			else
			{
				output.lookDirection = enemyBot->getExpectedPosition(m_tickCount % m_attackScanTick, targetDist) - input.position;
				output.action = BotOutput::shoot;

				Line attackPos;
				attackPos.start = input.position;
				attackPos.end = output.lookDirection;
				attackPos.r = 1;
				attackPos.g = 0;
				attackPos.b = 0;

				output.lines.push_back(attackPos);
			}

			break;
		}
	}

	//Output
	output.moveDirection = m_moveTarget - input.position;
	output.motor = 1.0;

	//Debug
	Line movePos;
	movePos.start = input.position;
	movePos.end = m_moveTarget;
	movePos.r = 0;
	movePos.g = 0;
	movePos.b = 1;

	output.lines.push_back(movePos);

	//Set our last action
	m_lastAction = output.action;

	//How to check a tile
	//m_initdata.mapData.data[int(input.position.x)+int(input.position.y)*m_initdata.mapData.width].wall
}

void Robot::result(bool won)
{
}
