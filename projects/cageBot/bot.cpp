#include "bot.h"
#include "time.h"

extern "C"
{
	BotInterface BOT_API *CreateBot()
	{
		return new Robot();
	}
}

//================================================================================
// Constructor/Initialisation
//================================================================================

Robot::Robot()
{
	m_rand(time(0) + (int)this);
}

void Robot::init(const BotInitialData &initialData, BotAttributes &attrib)
{
	//Reset our start status
	m_started = false;

	//Inital Game Data
	m_initdata = initialData;

	//Reset our look angle.
	m_lookAngle = 0.0f;

	//Bot Attributes. Determined based on which level has been loaded.
	attrib.health = 10.0;
	attrib.motor = 1.0;
	attrib.weaponSpeed = 1.0;
	attrib.weaponStrength = 7.0;

	//Generate map nodes based on the given initial map data.
	for (int i = 0; i < m_initdata.mapData.height; ++i)
	{
		for (int j = 0; j < m_initdata.mapData.width; ++j)
		{
			bool isWall = m_initdata.mapData.data[i * m_initdata.mapData.width + j].wall;
			m_mapNodes.push_back(new Node(j, i, isWall));
		}
	}
}

void Robot::result(bool won)
{
	//Round is over. Perform post round cleanup.

	//Clear our enemy list
	m_enemyBots.clear();

	//Clear map data
	m_mapNodes.clear();
}

//================================================================================
// Main update Loop
//================================================================================

void Robot::update(const BotInput &input, BotOutput &output)
{
	if (!m_started)
	{
		//This is our first tick. Do final init stuff.
		m_currentHealth = input.health;
		findNewPosition(input.position);
		m_started = true;
	}

	//Reset Debug Line Data
	output.lines.clear();

	//Notify Enemy bots that they have yet to be visible this update.
	for (EnemyBot* enemyBot : m_enemyBots)
	{
		enemyBot->update();
	}

	//Process Scan Results
	int visibleEnemyCount = processScanData(input);

	//Pathfinding
	calculateGoal(input, output);

	//Check current world/player states 
	updateBotState(input);

	//Do Bot Logic
	botLogic(input, output);

	//Increment Tick Count
	++m_tickCount;

	//Output
	output.moveDirection = m_nextPos - input.position;
	output.motor = 1.0;

	//Debug
	Line movePos;
	movePos.start = input.position;
	movePos.end = m_nextPos;
	movePos.r = 0;
	movePos.g = 0;
	movePos.b = 1;

	output.lines.push_back(movePos);

	//Set our last action
	m_lastAction = output.action;
}

//================================================================================
// Behaviours/Bot States
//================================================================================

void Robot::botLogic(const BotInput &input, BotOutput &output)
{
	switch (m_botState)
	{
	case FLEE:
	{
		//Switch move direction every "m_fleeSwitchTick" tick, or when we arrive at our target.
		if (m_tickCount % m_fleeSwitchTick == 0 || distance(m_moveTarget, input.position) < m_switchDist)
		{
			findNewPosition(input.position);
		}

		//Look back at our last known enemy location, and attack every "m_fleeAttackTick" ticks.
		if (m_tickCount % m_fleeAttackTick == 0 && m_enemyBot != nullptr)
		{
			output.lookDirection = m_enemyBot->getPos() - input.position;
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
		//Switch Direction when we arrive at our Target
		if (distance(m_moveTarget, input.position) < m_switchDist)
		{
			findNewPosition(input.position);
		}

		//Do a normal search
		output.lookDirection.set(cos(m_lookAngle), sin(m_lookAngle));
		m_lookAngle += m_initdata.scanFOV * 2;

		output.action = BotOutput::scan;

		break;
	}
	case ATTACK:
	{
		float targetDist = m_enemyBot->getDistanceToPostion(input.position);

		//Set our target position to the enemy if it is visible this update, and we are not too close.
		if (m_enemyBot->m_invisible == 0 && distance(m_enemyBot->getPos(), input.position) > m_attackSwitchDist)
		{
			m_moveTarget = m_enemyBot->getPos();
			m_targetType = ENEMY;
		}

		//Switch move direction when we become too close, or our current position is not moving us away enough
		else if ((m_targetType == ENEMY && distance(m_enemyBot->getPos(), input.position) < m_attackSwitchDist) || (distance(m_enemyBot->getPos(), input.position) < m_attackSwitchDist && m_tickCount % m_attackSwitchTick == 0))
		{
			findNewPosition(input.position);
		}

		//Scan every "m_attackScanTick" tick
		if (m_tickCount % m_attackScanTick == 0)
		{
			output.lookDirection.set(cos(m_lookAngle), sin(m_lookAngle));
			m_lookAngle += m_initdata.scanFOV * 2;
			output.action = BotOutput::scan;
		}

		//Scan last known location of enemy every "m_attackLastScanTick" ticks
		if (m_tickCount % m_attackLastScanTick == 0)
		{
			output.lookDirection = m_enemyBot->getExpectedPosition(m_tickCount % m_attackScanTick, targetDist) - input.position;
			output.action = BotOutput::scan;
		}

		//Shoot every other tick
		else
		{
			output.lookDirection = m_enemyBot->getExpectedPosition(m_tickCount % m_attackScanTick, targetDist) - input.position;
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
}

void Robot::updateBotState(const BotInput &input)
{
	BotState state = SEARCHING;
	m_enemyBot = getEnemyTarget(input.position);

	//Check if our bot has sustained damage.
	if (input.health < m_currentHealth)
	{
		m_damageTaken += m_currentHealth - input.health;
		m_currentHealth = input.health;
	}

	//We have sustained too much damage, or are still currently fleeing.
	if (m_damageTaken > m_fleeDmg || (m_botState == FLEE && m_enemyBot != nullptr && m_enemyBot->getDistanceToPostion(input.position) < m_fleeDist))
	{
		state = FLEE;
	}

	//We can see an enemy. Enter the attack state.
	else if (m_enemyBot != nullptr)
	{
		state = ATTACK;
	}

	//We have changed states. Reset our tick count and damage taken.
	if (state != m_botState)
	{
		m_botState = state;
		m_tickCount = 0;
		m_damageTaken = 0;
	}
}

//================================================================================
// Enemies/Scanning
//================================================================================

int Robot::processScanData(const BotInput &input)
{
	int visibleEnemyCount = 0;

	if (m_lastAction == BotOutput::scan)
	{
		for (auto scanResult : input.scanResult)
		{
			if (scanResult.type == VisibleThing::e_robot)
			{
				EnemyBot* bot = nullptr;

				for (EnemyBot* knownBot : m_enemyBots)
				{
					if (knownBot->m_botID == scanResult.name)
					{
						bot = knownBot;
						break;
					}
				}

				if (bot == nullptr)
				{
					m_enemyBots.push_back(new EnemyBot(scanResult.name));
					bot = m_enemyBots.back();
				}

				bot->setPos(scanResult.position);

				++visibleEnemyCount;
			}
		}
	}

	return visibleEnemyCount;
}

EnemyBot* Robot::getEnemyTarget(kf::Vector2 localPos)
{
	EnemyBot* targetBot = nullptr;
	float targetDist = 0.0f;

	for (EnemyBot* bot : m_enemyBots)
	{
		if (bot->m_invisible < m_enemyLostTickLimit)
		{
			float dist = bot->getDistanceToPostion(localPos);
			if (dist < targetDist || targetDist == 0.0f)
			{
				targetBot = bot;
				targetDist = dist;
			}
		}
	}

	return targetBot;
}

//================================================================================
// Pathfinding / Movement
//================================================================================

void Robot::findNewPosition(kf::Vector2 startPos)
{
	do
	{
		m_moveTarget.set(m_rand() % (m_initdata.mapData.width - 2) + 2, m_rand() % (m_initdata.mapData.height - 2) + 2);
	} while (m_initdata.mapData.data[int(m_moveTarget.x) + int(m_moveTarget.y)*m_initdata.mapData.width].wall || distance(startPos, m_moveTarget) < m_newTargetDist);

	m_targetType = POSITION;
}

int Robot::calculateAdjacentCost(Node* node)
{
	int adjacentCost = 0;

	for (int i = -1; i < 2; ++i)
	{
		for (int j = -1; j < 2; ++j)
		{
			//TODO Include or not include diagonal adjacents.

			int moveCost = (std::abs(i) + std::abs(j)) * 2;

			if (moveCost == 20) moveCost = 14;

			//This is a node which is adjacent above us, or to the left or right of us.
			if (moveCost != 0)
			{
				//Get our adjacent node positions
				int adjPosX = node->getX() + i;
				int adjPosY = node->getY() + j;

				//Check these positions don't go outside the map bounderies
				if (adjPosX >= 0 && adjPosX < m_initdata.mapData.width && adjPosY >= 0 && adjPosY < m_initdata.mapData.height)
				{
					Node* adjNode = m_mapNodes[adjPosX + m_initdata.mapData.width * adjPosY];

					//Increment the cost if adjacent node is a wall.
					if (adjNode->isWall())
					{
						adjacentCost += m_adjWallCost;
					}
				}
			}
		}
	}

	return adjacentCost;
}

void Robot::calculateGoal(const BotInput &input, BotOutput &output)
{
	//Reset our calculated node data.
	for (Node* node : m_mapNodes)
	{
		node->m_parent = nullptr;

		node->m_open = false;
		node->m_closed = false;

		node->m_fCost = 0;
		node->m_gCost = 0;
		node->m_hCost = 0;
	}

	std::vector<Node*> openNodes;

	//Finds the nodes containing our current, and target positions.
	for (Node* node : m_mapNodes)
	{
		if (node->getX() == (int)input.position.x && node->getY() == (int)input.position.y)
		{
			m_currentNode = node;
		}

		if (node->getX() == (int)m_moveTarget.x && node->getY() == (int)m_moveTarget.y)
		{
			m_targetNode = node;
		}
	}

	//Add our current node to the open list, and update its open status.
	m_currentNode->m_open = true;
	openNodes.push_back(m_currentNode);

	while (!openNodes.empty())
	{
		int minCost = openNodes[0]->m_fCost;
		Node* minCostNode = openNodes[0];

		for (Node* node : openNodes)
		{
			if (node->m_fCost < minCost)
			{
				minCost = node->m_fCost;
				minCostNode = node;
			}
		}

		m_currentNode = minCostNode;

		for (int i = -1; i < 2; ++i)
		{
			for (int j = -1; j < 2; ++j)
			{
				int moveCost = (std::abs(i) + std::abs(j));

				if (moveCost == 1)
				{
					int posX = m_currentNode->getX() + i;
					int posY = m_currentNode->getY() + j;

					Node* adjNode = m_mapNodes[posX + m_initdata.mapData.width * posY];

					//Check the adjacent nodes of the adjacent node. If the adjacents are walls, increase the travel cost for this node.
					int adjNodeCost = calculateAdjacentCost(adjNode);

					if (!adjNode->isWall() && !adjNode->m_closed)
					{
						int travelCost = m_currentNode->m_gCost + moveCost + adjNodeCost;

						if (!adjNode->m_open || (adjNode->m_open && travelCost < adjNode->m_gCost))
						{
							//Push this node into the open list if it does not already contain it.
							if (!adjNode->m_open)
							{
								openNodes.push_back(adjNode);
							}

							adjNode->m_gCost = travelCost;
							adjNode->m_open = true;
							adjNode->m_parent = m_currentNode;

							int heuristic = abs(posX - m_targetNode->getX()) + abs(posY - m_targetNode->getY());
							adjNode->m_hCost = heuristic;
							adjNode->m_fCost = adjNode->m_hCost + adjNode->m_gCost;
						}
					}
				}
			}
		}

		m_currentNode->m_open = false;
		m_currentNode->m_closed = true;

		for (int i = 0; i < openNodes.size(); ++i)
		{
			if (openNodes[i] == m_currentNode)
			{
				openNodes[i] = openNodes.back();
				openNodes.pop_back();
				break;
			}
		}

		if (m_currentNode == m_targetNode)
		{
			//We now have the required path data
			break;
		}
	}

	Node* currentNodeStep = m_targetNode;
	std::vector<Node*> path;

	while (currentNodeStep != nullptr)
	{
		path.insert(path.begin(), currentNodeStep);
		currentNodeStep = currentNodeStep->m_parent;
	}

	for (int i = 0; i < path.size() - 1; ++i)
	{
		Line pathLine;
		pathLine.start = kf::Vector2(path[i]->getX() + 0.5, path[i]->getY() + 0.5);
		pathLine.end = kf::Vector2(path[i + 1]->getX() + 0.5, path[i + 1]->getY() + 0.5);;
		pathLine.r = 0;
		pathLine.g = 1;
		pathLine.b = 0;

		output.lines.push_back(pathLine);
	}

	if (path.size() > 1)
	{
		m_nextPos = kf::Vector2(path[1]->getX() + 0.5f, path[1]->getY() + 0.5f);
	}
}

//================================================================================
// Utils
//================================================================================

float Robot::distance(kf::Vector2 pos1, kf::Vector2 pos2)
{
	float diffY = pos1.y - pos2.y;
	float diffX = pos1.x - pos2.x;
	return sqrt((diffY * diffY) + (diffX * diffX));
}
